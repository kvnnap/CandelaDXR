#include "Utils.hlsli"
#include "Scene.hlsli"

struct ConstBuff
{
	uint2 seeds;
	uint2 winDim;
	uint numLights;
	uint frameNumber;
};

struct ShadowPayload
{
	bool occluded;
};

RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float4> gRadiance : register(u1);

// SRVs
StructuredBuffer<float3> verts : register(t0);
StructuredBuffer<float2> texVerts : register(t1);
StructuredBuffer<float3> normals : register(t2);
StructuredBuffer<uint> indices : register(t3);
StructuredBuffer<float4x3> matrices : register(t4);
StructuredBuffer<float3x3> normalMatrices : register(t5);
StructuredBuffer<FaceAttributes> faceAttributes : register(t6);
StructuredBuffer<Material> materials : register(t7);
StructuredBuffer<AreaLight> lights : register(t8);

RaytracingAccelerationStructure gRtScene : register(t10);
Texture2D<float4> gPos : register(t11);
Texture2D<float4> gNorm : register(t12);

Texture2D<float3> gTextures[]: register(t14);

// Sampler
SamplerState gSampler : register(s0);

// CBVs
cbuffer CB1 : register(b0)
{
	ConstBuff cBuffer;
}

// Util Functions
#include "RayTracingUtils.hlsli"

// Kernels

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x, y point
	const uint2 launchIndex = DispatchRaysIndex().xy;

	// Dimensions - the previous x,y point is contained within these dimensions
	const uint2 launchDim = DispatchRaysDimensions().xy;

	// Flat dimensions
	const uint flatLaunchIndex = launchIndex.y * launchDim.x + launchIndex.x;

	// Early-exit checks
	if (cBuffer.numLights == 0 || gOutput[launchIndex].w == 0.f)
		return;

	// Initialise seed
	uint seed = rand_init(
		cBuffer.seeds.x + launchDim.x * (cBuffer.frameNumber + 0) + launchIndex.x,
		cBuffer.seeds.y + launchDim.y * (cBuffer.frameNumber + 0) + launchIndex.y);

	const uint lightIndex = chooseInRange(seed, 0, cBuffer.numLights - 1);
	const uint lightIndexId = lights[lightIndex].PrimitiveId * 3;
	AreaLight areaLight = lights[lightIndex];
	float3 lv[3];
	getVertexWorldCoordinates(lv, lightIndexId, areaLight.InstanceIndex);
	float2 lightBary;
	const float3 pointOnLightSource = samplePointOnTriangle(seed, lv, lightBary);
	const float3 unitLightNormal = getUnitNormal(lightBary, lightIndexId, areaLight.InstanceIndex);

	// Construct Ray
	RayDesc shadowRay;
	shadowRay.TMin = 0.001f;
	shadowRay.TMax = 0.999f;
	shadowRay.Origin = gPos[launchIndex].xyz;
	shadowRay.Direction = pointOnLightSource - shadowRay.Origin;

	// Constants
	const float3 unitFaceNormal = normalize(gNorm[launchIndex].xyz);
	const float invShadowDistance = 1.f / length(shadowRay.Direction);
	const float3 unitShadowRayDirection = shadowRay.Direction * invShadowDistance;
	const float lightDot = -dot(unitShadowRayDirection, unitLightNormal);
	const float triangleArea = getTriangleArea(lv);
	const float surfaceLightDot = dot(unitShadowRayDirection, unitFaceNormal);

	float3 radiance = gOutput[launchIndex].xyz;

	if (lightDot > 0.f && surfaceLightDot > 0.f)
	{
		ShadowPayload shadowPayload;
		shadowPayload.occluded = true;
		TraceRay(
			gRtScene,	// Acceleration Structure
			RAY_FLAG_FORCE_OPAQUE
			| RAY_FLAG_SKIP_CLOSEST_HIT_SHADER
			| RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,			// Ray flags
			0xFF,		// Instance inclusion Mask (0xFF includes everything)
			0,			// RayContributionToHitGroupIndex (calls shadowAnyHit)
			0,			// MultiplierForGeometryContributionToShaderIndex (We only have 1 hit group)
			0,			// Miss shader index (within the shader table) (calls shadowMiss)
			shadowRay,
			shadowPayload);
		if (shadowPayload.occluded)
		{
			radiance = 0.f;
		}
	}

	// Using this resource as a RADIANCE accumulator
	if (cBuffer.frameNumber == 1)
		gRadiance[launchIndex] = float4(0.f, 0.f, 0.f, 0.f);
	gRadiance[launchIndex] += float4(radiance, 0.f);
	gOutput[launchIndex] = float4(gRadiance[launchIndex].xyz / cBuffer.frameNumber, 1.f);
}

// Shadow
[shader("miss")]
void shadowMiss(inout ShadowPayload payload)
{
	payload.occluded = false;
}
