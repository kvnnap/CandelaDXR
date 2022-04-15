#include "Utils.hlsli"

cbuffer CB1 : register(b0)
{
	uint InIndex;
	uint OutIndex;
	uint Clear;
	uint LinearToSRGB;
}

RWTexture2D<float4> gArr[] : register(u0);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if (Clear)
		gArr[OutIndex][DTid.xy] = float4(0.f, 0.f, 0.f, 0.f);
	gArr[OutIndex][DTid.xy] += gArr[InIndex][DTid.xy];
	if (LinearToSRGB)
		gArr[OutIndex][DTid.xy] = float4(linearToSrgb(toneMap(gArr[OutIndex][DTid.xy].xyz)), 1.f);
}

