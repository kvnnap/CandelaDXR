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

// CBVs
cbuffer CB1 : register(b0)
{
	ConstBuff cBuffer;
}

// Output texture
RWStructuredBuffer<IrradianceItem> gIrradianceDS : register(u1);

// Kernels
[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x, y point
	const uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 lInd = min(launchIndex, cBuffer.winDim);
	const uint pixLaunchIndex = lInd.y * cBuffer.winDim.x + lInd.x;
	gIrradianceDS[pixLaunchIndex].value = floatToFixed(float3(1.f, 0.f, 0.f), ConvRangeBits);
}

// Ray
[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
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
