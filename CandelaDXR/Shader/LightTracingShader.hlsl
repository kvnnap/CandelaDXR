#include "Utils.hlsli"
#include "Scene.hlsli"
#include "IrradianceItem.hlsli"

struct ConstBuff 
{
	float3 u, v, w;
	float3 position;
	float3 direction;
	float3 plane; // sensor dimensions (z contains distance to sensor plane)
	uint2 seeds;
	uint2 winDim;
	uint numLights;
	uint numSpeculars;
	uint frameNumber;
	uint padding;
};

struct RayPayload
{
	float2 bary;
	float t;
	uint faceIndex;
};

struct ShadowPayload
{
	bool occluded;
};

// UAVs

// Output texture
RWTexture2D<float4> gOutput : register(u0);
RWStructuredBuffer<IrradianceItem> gIrradianceDS : register(u1);

// SRVs
StructuredBuffer<float3> verts : register(t0);
StructuredBuffer<float2> texVerts : register(t1);
StructuredBuffer<float3> normals : register(t2);
StructuredBuffer<uint> indices : register(t3);
StructuredBuffer<float4x3> matrices : register(t4);
StructuredBuffer<FaceAttributes> faceAttributes : register(t5);
StructuredBuffer<Material> materials : register(t6);
StructuredBuffer<AreaLight> lights : register(t7);
StructuredBuffer<SpecularPrimitive> speculars : register(t8);

RaytracingAccelerationStructure gRtScene : register(t9);

Texture2D<float> gIrrToRad : register(t10);
Texture2D<float3> gTextures[]: register(t11);

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

float3 getUnitNormal(float2 bary, uint vertBaseId, uint matrixId)
{
	float3 ln[3];
	ln[0] = mul(float4(normals[indices[vertBaseId + 0]], 0.f), matrices[matrixId]);
	ln[1] = mul(float4(normals[indices[vertBaseId + 1]], 0.f), matrices[matrixId]);
	ln[2] = mul(float4(normals[indices[vertBaseId + 2]], 0.f), matrices[matrixId]);
	return normalize(interpolateVertices(bary, ln));
}

float2 getTextureLocation(float2 bary, uint vertBaseId)
{
	float2 lt[3];
	lt[0] = texVerts[indices[vertBaseId + 0]];
	lt[1] = texVerts[indices[vertBaseId + 1]];
	lt[2] = texVerts[indices[vertBaseId + 2]];
	return pointOnTriangle(bary, lt);
}

void getVertexWorldCoordinates(inout float3 lv[3], uint vertBaseId, uint matrixId)
{
	lv[0] = mul(float4(verts[indices[vertBaseId + 0]], 1.f), matrices[matrixId]);
	lv[1] = mul(float4(verts[indices[vertBaseId + 1]], 1.f), matrices[matrixId]);
	lv[2] = mul(float4(verts[indices[vertBaseId + 2]], 1.f), matrices[matrixId]);
}

void AddContribution(uint pixLaunchIndex, float3 contrib)
{
	uint3 uContrib = floatToFixed(contrib, ConvRangeBits);
	InterlockedAdd(gIrradianceDS[pixLaunchIndex].value.x, uContrib.x);
	InterlockedAdd(gIrradianceDS[pixLaunchIndex].value.y, uContrib.y);
	InterlockedAdd(gIrradianceDS[pixLaunchIndex].value.z, uContrib.z);
}

// Kernels

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x, y point
	const uint2 launchIndex = DispatchRaysIndex().xy;

	// Dimensions - the previous x,y point is contained within these dimensions
	const uint2 launchDim = DispatchRaysDimensions().xy;

	// Early-exit checks
	if (cBuffer.numLights == 0)
		return;
	
	// Initialise seed
	uint seed = rand_init(
		cBuffer.seeds.x + launchDim.x * (cBuffer.frameNumber + 0) + launchIndex.x,
		cBuffer.seeds.y + launchDim.y * (cBuffer.frameNumber + 0) + launchIndex.y);

	// Choose light source
	const uint lightIndex = chooseInRange(seed, 0, cBuffer.numLights - 1);
	const uint lightIndexId = lights[lightIndex].PrimitiveId * 3;
	AreaLight areaLight = lights[lightIndex];
	Material lightMat = materials[areaLight.MaterialId];

	// Compute light vertices
	float3 lv[3];
	getVertexWorldCoordinates(lv, lightIndexId, areaLight.InstanceIndex);

	// Generate a point on the light
	float2 lightBary;
	const float3 pointOnLightSource = samplePointOnTriangle(seed, lv, lightBary);

	// Compute MC Coefficients
	float3 localContribution = lightMat.Emissive;
	localContribution *= getTriangleArea(lv) * cBuffer.numLights;

	if (lightMat.EmissiveTextureId >= 0)
		localContribution *= gTextures[lightMat.EmissiveTextureId].SampleLevel(gSampler, getTextureLocation(lightBary, lightIndexId), 0);

	// First check if light normal is the right way round wrt camera
	const float3 unitLightNormal = getUnitNormal(lightBary, lightIndexId, areaLight.InstanceIndex);

	// Construct ray from light source to camera origin
	RayDesc shadowRay;
	shadowRay.TMin = 0.001f;
	shadowRay.TMax = 1.f;
	shadowRay.Origin = pointOnLightSource;
	shadowRay.Direction = cBuffer.position - pointOnLightSource;
	float invShadowDistance = 1.f / length(shadowRay.Direction);
	float3 unitShadowRayDirection = shadowRay.Direction * invShadowDistance;

	uint2 pixel = uint2(0, 0);
	float lightDot = dot(unitShadowRayDirection, unitLightNormal);
	float cameraDot = -dot(unitShadowRayDirection, cBuffer.w);

	ShadowPayload shadowPayload;

	if (lightDot > 0.f && cameraDot > 0.f && getPixel(shadowRay, cBuffer.winDim, pixel))
	{
		// Add direct light contribution
		shadowPayload.occluded = true;
		TraceRay(
			gRtScene,	// Acceleration Structure
			RAY_FLAG_FORCE_OPAQUE
		  | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER
		  | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,			// Ray flags
			0xFF,		// Instance inclusion Mask (0xFF includes everything)
			1,			// RayContributionToHitGroupIndex (calls shadowAnyHit)
			1,			// MultiplierForGeometryContributionToShaderIndex (We only have 1 hit group)
			1,			// Miss shader index (within the shader table) (calls shadowMiss)
			shadowRay,
			shadowPayload);
		
		if (!shadowPayload.occluded)
		{
			const uint pixLaunchIndex = pixel.y * cBuffer.winDim.x + pixel.x;
			float3 contrib = localContribution * lightDot * invShadowDistance * invShadowDistance * cameraDot;
			AddContribution(pixLaunchIndex, contrib);
		}
	}

	// Construct ray from light source to random scene point
	float pdf;
	RayDesc ray;
	ray.TMin = 0.001f;
	ray.TMax = 3.402823e+38;
	ray.Origin = shadowRay.Origin;

	ray.Direction = randomRayLobe(seed, unitLightNormal, 1, pdf);
	localContribution *= dot(unitLightNormal, ray.Direction) / pdf;

	// Number of entries in transmissive materials
	int numEntries = 0;

	// Traverse scene to another surface
	uint i = 0;
	RayPayload rayPayload;
	while (TraceRay(
		gRtScene,	// Acceleration Structure
		0,			// Ray flags
		0xFF,		// Instance inclusion Mask (0xFF includes everything)
		0,			// RayContributionToHitGroupIndex (calls chs)
		1,			// MultiplierForGeometryContributionToShaderIndex
		0,			// Miss shader index (within the shader table) (calls miss)
		ray,
		rayPayload), rayPayload.t != 0.f)
	{ 
		float3 intersectionPoint = ray.Origin + rayPayload.t * ray.Direction;

		// Get Face attributes
		FaceAttributes fAttr = faceAttributes[rayPayload.faceIndex];

		const uint vertIndex = rayPayload.faceIndex * 3;

		// Get face unit normal
		float3 unitFaceNormal = getUnitNormal(rayPayload.bary, vertIndex, fAttr.InstanceIndex);
		float wiDot = dot(ray.Direction, unitFaceNormal);

		// Get appropriate BRDF - assuming Diffuse (Will handle Reflective and Transmissive later)
		Material mat = materials[fAttr.MaterialId];

		const bool isInternal = wiDot > 0.f;

		// Beer's law
		if (isInternal)
		{
			if (numEntries <= 0)
				return;
			localContribution *= exp((-rayPayload.t) * mat.TransmissiveFilter);
		}
		else
		{
			// Check contribution to eye
			shadowRay.Origin = intersectionPoint;
			shadowRay.Direction = cBuffer.position - intersectionPoint;
			invShadowDistance = 1.f / length(shadowRay.Direction);
			unitShadowRayDirection = shadowRay.Direction * invShadowDistance;
			float surfaceDot = dot(unitShadowRayDirection, unitFaceNormal);
			cameraDot = -dot(unitShadowRayDirection, cBuffer.w);

			if (surfaceDot > 0.f && cameraDot > 0.f && getPixel(shadowRay, cBuffer.winDim, pixel))
			{
				shadowPayload.occluded = true;
				TraceRay(
					gRtScene,	// Acceleration Structure
					RAY_FLAG_FORCE_OPAQUE
					| RAY_FLAG_SKIP_CLOSEST_HIT_SHADER
					| RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,			// Ray flags
					0xFF,		// Instance inclusion Mask (0xFF includes everything)
					1,			// RayContributionToHitGroupIndex (calls shadowAnyHit)
					1,			// MultiplierForGeometryContributionToShaderIndex (We only have 1 hit group)
					1,			// Miss shader index (within the shader table) (calls shadowMiss)
					shadowRay,
					shadowPayload);

				if (!shadowPayload.occluded)
				{
					const uint pixLaunchIndex = pixel.y * cBuffer.winDim.x + pixel.x;
					float3 brdfDiff = mat.Diffuse * OneOverPI;
					if (mat.DiffuseTextureId >= 0)
						brdfDiff *= gTextures[mat.DiffuseTextureId].SampleLevel(gSampler, getTextureLocation(rayPayload.bary, vertIndex), 0);
					float3 contrib = (localContribution * brdfDiff) * mat.Dissolve * surfaceDot * invShadowDistance * invShadowDistance * cameraDot;
					AddContribution(pixLaunchIndex, contrib);
				}
			}
		}

		// Russian roulette
		if (++i >= 3)
		{
			const float probabilityOfContinuing = 0.5f;
			if (rand_next(seed) > probabilityOfContinuing)
				break;
			localContribution *= 1.f / probabilityOfContinuing;
		}

		// Setup Fresnel coeff
		float n1, n2, dissolve, coeff;
		if (isInternal)
		{
			n1 = mat.RefractiveIndex;
			n2 = 1.f;
			dissolve = 0.f;
			coeff = -1.f;
		}
		else
		{
			n1 = 1.f;
			n2 = mat.RefractiveIndex;
			dissolve = mat.Dissolve;
			coeff = 1.f;
		}

		ray.Origin = intersectionPoint;

		// Compute Fresnel
		float fr = fresnel(-coeff * wiDot, n1, n2); // Reflection 
		if (rand_next(seed) < fr)
		{
			ray.Direction = reflect(ray.Direction, coeff * unitFaceNormal);
			continue;
		}

		// Diffuse?
		if (rand_next(seed) < dissolve)
		{
			// Sample the brdf and generate a new ray
			ray.Direction = randomRayLobe(seed, unitFaceNormal, 1, pdf);
			float3 brdfDiff = mat.Diffuse * OneOverPI;
			if (mat.DiffuseTextureId >= 0)
				brdfDiff *= gTextures[mat.DiffuseTextureId].SampleLevel(gSampler, getTextureLocation(rayPayload.bary, vertIndex), 0);
			localContribution *= brdfDiff * dot(unitFaceNormal, ray.Direction) / pdf;
		}
		else
		{
			// Transmission
			float3 dir = refract(ray.Direction, coeff * unitFaceNormal, n1 / n2);
			if (any(dir))
			{
				ray.Direction = dir;
				numEntries += isInternal ? -1 : 1;
			}
			else
			{
				ray.Direction = reflect(ray.Direction, coeff * unitFaceNormal);
			}
		}
	}
}

// Ray
[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.bary = attribs.barycentrics;
	payload.t = RayTCurrent();
	payload.faceIndex = getFaceIndex();
}

[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.t = 0.f;
}

// Shadow
[shader("miss")]
void shadowMiss(inout ShadowPayload payload)
{
	payload.occluded = false;
}
