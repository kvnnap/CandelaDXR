#include "Utils.hlsli"
#include "IrradianceItem.hlsli"

cbuffer CB1 : register(b0)
{
	uint2 ScreenDim;
	uint LightSamples;
	uint FrameNumber;
	uint Clear;
}

RWTexture2D<float4> gOutput : register(u0);
RWStructuredBuffer<IrradianceItem> gIrradianceDS : register(u1);
RWTexture2D<float4> gIrradiance : register(u2);
RWTexture2D<float4> gRayHitT : register(u3);

// Caustics
RWTexture2D<float4> gIrradianceCaustics : register(u4);
RWTexture2D<float4> gOutputCaustics : register(u5);

Texture2D<float> gIrrToRad : register(t0);

// 64 threads per group should be optimal on both NVIDIA and AMD
// i.e. 2 warps per thread group or 1 depending on NVIDIA/AMD
// 32 threads per group might be better on NVIDIA.. no. It seems one SM can dispatch
// more than 1 warp concurrently..? Not sure.
[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if (DTid.x >= ScreenDim.x || DTid.y >= ScreenDim.y)
		return;

	if (Clear)
		gIrradiance[DTid.xy] = gIrradianceCaustics[DTid.xy] = float4(0.f, 0.f, 0.f, 0.f);

	const uint flatLaunchIndex = DTid.y * ScreenDim.x + DTid.x;
	float4 result = fixedToFloat(gIrradianceDS[flatLaunchIndex].value, ConvRangeBits);
	float4 caust  = fixedToFloat(gIrradianceDS[flatLaunchIndex].caust, ConvRangeBits);
	gIrradiance[DTid.xy] += result;
	gIrradianceCaustics[DTid.xy] += caust;
	gIrradianceDS[flatLaunchIndex].value = 0;
	gIrradianceDS[flatLaunchIndex].caust = 0;

	const float sampleRatio = 1.f / (LightSamples * (float)FrameNumber);
	const float coeff = gIrrToRad[DTid.xy] * sampleRatio;
	float4 prevRayHitT = gRayHitT[DTid.xy];
	prevRayHitT.w = 1.f; // Remove before commit - this serves for viewing the texture
	gRayHitT[DTid.xy] = float4(gIrradiance[DTid.xy].w * coeff, prevRayHitT.y, gIrradianceCaustics[DTid.xy].w, prevRayHitT.w);
	gOutput[DTid.xy] = float4(gIrradiance[DTid.xy].xyz * coeff, 1.f);
	gOutputCaustics[DTid.xy] = float4(gIrradianceCaustics[DTid.xy].xyz * coeff, 1.f);
}