#include "Utils.hlsli"

cbuffer CB1 : register(b0)
{
	uint InIndex;
	uint OutIndex;
	uint Clear;
	uint Accumulate;
	uint ToneMap;
	uint LinearToSRGB;
}

RWTexture2D<float4> gArr[] : register(u0);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if (Clear)
		gArr[OutIndex][DTid.xy] = float4(0.f, 0.f, 0.f, 0.f);
	if (Accumulate)
		gArr[OutIndex][DTid.xy] += gArr[InIndex][DTid.xy];
	if (ToneMap)
		gArr[OutIndex][DTid.xy] = float4(toneMap(gArr[OutIndex][DTid.xy].xyz), 1.f);
	if (LinearToSRGB)
		gArr[OutIndex][DTid.xy] = float4(linearToSrgb(gArr[OutIndex][DTid.xy].xyz), 1.f);
}

