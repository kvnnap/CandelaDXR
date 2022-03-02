#include "Utils.hlsli"
#include "IrradianceItem.hlsli"

cbuffer CB1 : register(b0)
{
	uint ScreenWidth;
	uint ScreenHeight;
	uint FrameNumber;
	uint Clear;
}

RWTexture2D<float4> gOutput : register(u0);
RWStructuredBuffer<IrradianceItem> gIrradianceDS : register(u1);
RWTexture2D<float4> gIrradiance : register(u2);

Texture2D<float> gIrrToRad : register(t0);

// 64 threads per group should be optimal on both NVIDIA and AMD
// i.e. 2 warps per thread group or 1 depending on NVIDIA/AMD
// 32 threads per group might be better on NVIDIA.. no. It seems one SM can dispatch
// more than 1 warp concurrently..? Not sure.
[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if (DTid.x >= ScreenWidth || DTid.y >= ScreenHeight)
		return;

	if (Clear)
		gIrradiance[DTid.xy] = float4(0.f, 0.f, 0.f, 0.f);

	//const uint resY = (DTid.y / 8) % 3;
	//const uint result = ((DTid.x / 8) + resY) % 3;
	//float redValue		= result == 0 ? 1.f : 0.f;
	//float blueValue		= result == 1 ? 1.f : 0.f;
	//float greenValue	= result == 2 ? 1.f : 0.f;
	//gOutput[DTid.xy] = float4(redValue, blueValue, greenValue, 1.f);
	const uint flatLaunchIndex = DTid.y * ScreenWidth + DTid.x;
	uint size = min(gIrradianceDS[flatLaunchIndex].counter, 16);
	for (uint i = 0; i < size; ++i)
		gIrradiance[DTid.xy] += gIrradianceDS[flatLaunchIndex].irradiance[i];
	
	
	const float sampleRatio = 1.f / (ScreenWidth * ScreenHeight * FrameNumber);
	gOutput[DTid.xy] = float4(linearToSrgb(toneMap(gIrradiance[DTid.xy].xyz * gIrrToRad[DTid.xy] * sampleRatio)), 1.f);
	//gOutput[DTid.xy] = float4((float)gIrradianceDS[flatLaunchIndex].counter / 256, 0.f, 0.f, 0.f);
	gIrradianceDS[flatLaunchIndex].counter = 0;
}