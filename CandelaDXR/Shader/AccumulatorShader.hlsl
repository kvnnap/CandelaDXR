#include "Utils.hlsli"
#include "AccumulatorShader.hlsli"

cbuffer CB1 : register(b0)
{
	uint InIndex;
	uint OutIndex;
	uint Flags;
}

RWTexture2D<float4> gArr[] : register(u0);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if (ACC_IS_SET(Flags, ACC_CLEAR))
		gArr[OutIndex][DTid.xy] = float4(0.f, 0.f, 0.f, 0.f);
	if (ACC_IS_SET(Flags, ACC_ACCUMULATE))
		gArr[OutIndex][DTid.xy] += gArr[InIndex][DTid.xy];
	if (ACC_IS_SET(Flags, ACC_TONEMAP))
		gArr[OutIndex][DTid.xy] = float4(toneMap(gArr[OutIndex][DTid.xy].xyz), 1.f);
	if (ACC_IS_SET(Flags, ACC_LINEARTOSRGB))
		gArr[OutIndex][DTid.xy] = float4(linearToSrgb(gArr[OutIndex][DTid.xy].xyz), 1.f);
}

