#include "Utils.hlsli"
#include "Scene.hlsli"

struct ConstBuff 
{
	float3 u, v, w;
	float3 position;
	float3 direction;
	float3 plane; // sensor dimensions (z contains distance to sensor plane)
	uint2 seeds;
	uint numLights;
	uint clear;
};

struct RayPayload
{
	float3 color;
};

struct ShadowPayload
{
	bool hit;
};

struct IndirectPayload
{
	float3 test;
};

// UAVs

// Output texture
RWTexture2D<float4> gOutput : register(u0);
globallycoherent RWTexture2D<float4> gIrradiance : register(u1);

// SRVs
StructuredBuffer<float3> verts : register(t0);
StructuredBuffer<float2> texVerts : register(t1);
StructuredBuffer<float3> normals : register(t2);
StructuredBuffer<uint> indices : register(t3);
StructuredBuffer<float4x3> matrices : register(t4);
StructuredBuffer<FaceAttributes> faceAttributes : register(t5);
StructuredBuffer<Material> materials : register(t6);
StructuredBuffer<AreaLight> lights : register(t7);

RaytracingAccelerationStructure gRtScene : register(t8);

Texture2D<float> gIrrToRad : register(t9);
Texture2D<float3> gTextures[]: register(t10);

// Sampler
SamplerState gSampler : register(s0);

// CBVs
cbuffer CB1 : register(b0)
{
	ConstBuff cBuffer;
}

// Functions
uint getFaceIndex()
{
	return InstanceID() / 3 + PrimitiveIndex();
}

bool getPixel(RayDesc ray, uint2 screenDimensions, inout uint2 pixel)
{
	//compute denominator
	const float den = dot(cBuffer.w, ray.Direction);

	// make this check or else risk of division by zero..
	if (den == 0.f)
		return false;

	const float3 posPlane = cBuffer.position + cBuffer.w * cBuffer.plane.z;
	const float t = -dot(cBuffer.w, (ray.Origin - posPlane)) / den;
	if (t < ray.TMin || t > ray.TMax)
		return false;

	const float3 R = ray.Origin + ray.Direction * t - posPlane;
	float2 pt = float2(dot(R, cBuffer.u), dot(R, cBuffer.v));
	pt /= cBuffer.plane.xy;
	pt += float2(0.5f, 0.5f);
	pt *= float2(screenDimensions);
	pt.y = float(screenDimensions.y) - pt.y;
	if (pt.x < 0.f || pt.y < 0.f || pt.x >= float(screenDimensions.x) || pt.y >= float(screenDimensions.y))
		return false;

	pixel = uint2(pt);
	return true;
}

// Kernels

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x, y point
	const uint2 launchIndex = DispatchRaysIndex().xy;

	// Dimensions - the previous x,y point is contained within these dimensions
	const uint2 launchDim = DispatchRaysDimensions().xy;

	//uint2 r = uint2(800, 500);
	//gOutput[r] = float4(1.f, 1.f, 1.f, 1.f);

	// Early-exit checks
	if (cBuffer.numLights == 0)
	{
		gOutput[launchIndex] = float4(0.f, 0.f, 0.f, 0.f);
		return;
	}

	// Clear buffer if stuff changed
	if (cBuffer.clear)
	{
		gIrradiance[launchIndex] = float4(0.f, 0.f, 0.f, 0.f);
		gOutput[launchIndex] = float4(0.f, 0.f, 0.f, 0.f);
	}
	
	// Initialise seed
	uint seed = rand_init(
		cBuffer.seeds.x + launchDim.x * ((uint)gIrradiance[launchIndex].w + 0) + launchIndex.x,
		cBuffer.seeds.y + launchDim.y * ((uint)gIrradiance[launchIndex].w + 0) + launchIndex.y);

	// Choose light source
	const uint lightIndex = chooseInRange(seed, 0, cBuffer.numLights - 1);
	const uint lightIndexId = lights[lightIndex].PrimitiveId * 3;
	AreaLight areaLight = lights[lightIndex];
	Material lightMat = materials[areaLight.MaterialId];

	// Compute light vertices
	float3 l[3];
	l[0] = mul(float4(verts[indices[lightIndexId + 0]], 1.f), matrices[areaLight.InstanceIndex]);
	l[1] = mul(float4(verts[indices[lightIndexId + 1]], 1.f), matrices[areaLight.InstanceIndex]);
	l[2] = mul(float4(verts[indices[lightIndexId + 2]], 1.f), matrices[areaLight.InstanceIndex]);

	// Generate a point on the light
	float2 lightBary;
	const float3 pointOnLightSource = samplePointOnTriangle(seed, l, lightBary);

	// Compute MC Coefficients
	float3 localContribution = lightMat.Emissive;
	localContribution *= getTriangleArea(l) * cBuffer.numLights;

	if (lightMat.EmissiveTextureId >= 0)
	{
		float2 lt[3];
		lt[0] = texVerts[indices[lightIndexId + 0]];
		lt[1] = texVerts[indices[lightIndexId + 1]];
		lt[2] = texVerts[indices[lightIndexId + 2]];
		localContribution *= gTextures[lightMat.EmissiveTextureId].SampleLevel(gSampler, pointOnTriangle(lightBary, lt), 0);
	}

	// Construct ray from light source to camera origin
	RayDesc ray;
	ray.TMin = 0.001f;
	ray.TMax = 1.f;
	ray.Origin = pointOnLightSource;
	ray.Direction = cBuffer.position - pointOnLightSource;

	// First check if light normal is the right way round wrt camera
	float3 ln[3];
	ln[0] = mul(float4(normals[indices[lightIndexId + 0]], 0.f), matrices[areaLight.InstanceIndex]);
	ln[1] = mul(float4(normals[indices[lightIndexId + 1]], 0.f), matrices[areaLight.InstanceIndex]);
	ln[2] = mul(float4(normals[indices[lightIndexId + 2]], 0.f), matrices[areaLight.InstanceIndex]);
	float3 lightNormal = interpolateVertices(lightBary, ln);
	

	uint2 pixel = uint2(0, 0);
	if (dot(ray.Direction, lightNormal) > 0.f && getPixel(ray, launchDim, pixel))
	{
		RayPayload payload;
		// Add direct contribution
		TraceRay(
			gRtScene,	// Acceleration Structure
			0,			// Ray flags
			0xFF,		// Instance inclusion Mask (0xFF includes everything)
			1,			// RayContributionToHitGroupIndex (calls shadowAnyHit)
			3,			// MultiplierForGeometryContributionToShaderIndex
			1,			// Miss shader index (within the shader table) (calls shadowMiss)
			ray,
			payload);
		
		/*if (payload.hit)
			gIrradiance[pixel].xyz = localContribution;
		else
			gIrradiance[pixel].xyz = float3(1.f, 0.f, 0.f);*/
		gIrradiance[pixel].xyz = payload.color;
	}

	//DeviceMemoryBarrier();

	//++gIrradiance[launchIndex].w;
	gOutput[launchIndex] = float4(gIrradiance[launchIndex].xyz, 1.f);
}

// Ray
[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(0.f, 1.f, 0.f);
}

[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.color = float3(0.f, 1.f, 1.f);
}

// Shadow
[shader("anyhit")]
void shadowAnyHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(1.f, 0.f, 0.f);

	//payload.hit = false;
	AcceptHitAndEndSearch();
}

[shader("miss")]
void shadowMiss(inout RayPayload payload)
{
	payload.color = float3(1.f, 1.f, 0.f);
	//payload.hit = false;
}

// Indirect
[shader("closesthit")]
void indirectChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(1.f, 1.f, 1.f);
}

[shader("miss")]
void indirectMiss(inout RayPayload payload)
{
	payload.color = float3(0.f, 1.f, 0.f);
}