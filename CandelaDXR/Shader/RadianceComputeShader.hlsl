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
RWTexture2D<float2> gRayHitT : register(u3);

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
		gIrradiance[DTid.xy] = float4(0.f, 0.f, 0.f, 0.f);

	const uint flatLaunchIndex = DTid.y * ScreenDim.x + DTid.x;
	float4 result = fixedToFloat(gIrradianceDS[flatLaunchIndex].value, ConvRangeBits);
	gIrradiance[DTid.xy] += result;
	gIrradianceDS[flatLaunchIndex].value = 0;

	const float sampleRatio = 1.f / (LightSamples * (float)FrameNumber);
	float2 prevRayHitT = gRayHitT[DTid.xy];
	gRayHitT[DTid.xy] = float2(gIrradiance[DTid.xy].w * gIrrToRad[DTid.xy] * sampleRatio, prevRayHitT.y);
	gOutput[DTid.xy] = float4(gIrradiance[DTid.xy].xyz * gIrrToRad[DTid.xy] * sampleRatio, 1.f);
}