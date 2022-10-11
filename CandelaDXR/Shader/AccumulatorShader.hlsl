#include "Utils.hlsli"
#include "AccumulatorShader.hlsli"

cbuffer CB1 : register(b0)
{
	uint4 InIndex;
	uint4 OutIndex;
	uint PairCount;
	uint Flags;
}

RWTexture2D<float4> gArr[] : register(u0);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	for (uint i = 0; i < PairCount; ++i)
	{
		uint inIdx = InIndex[i];
		uint outIdx = OutIndex[i];
		if (ACC_IS_SET(Flags, ACC_CLEAR))
			gArr[outIdx][DTid.xy] = float4(0.f, 0.f, 0.f, 0.f);
		if (ACC_IS_SET(Flags, ACC_ACCUMULATE))
			gArr[outIdx][DTid.xy] += gArr[inIdx][DTid.xy];
		if (ACC_IS_SET(Flags, ACC_TONEMAP))
			gArr[outIdx][DTid.xy] = float4(toneMap(gArr[outIdx][DTid.xy].xyz), 1.f);
		if (ACC_IS_SET(Flags, ACC_LINEARTOSRGB))
			gArr[outIdx][DTid.xy] = float4(linearToSrgb(gArr[outIdx][DTid.xy].xyz), 1.f);
	}
}

