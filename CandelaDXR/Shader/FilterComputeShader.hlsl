cbuffer CB1 : register(b0)
{
	uint2 ScreenDim;
	uint InputIndex;
	uint OutputIndex;
}

Texture2D<float> input[] : register(t0);
RWTexture2D<float> output[] : register(u0);
RWTexture2D<float> scratch : register(u0, space1);


cbuffer CB1 : register(b1)
{
	float3 Position;
	float3 Plane;
}

// 64 threads per group should be optimal on both NVIDIA and AMD
// i.e. 2 warps per thread group or 1 depending on NVIDIA/AMD
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= ScreenDim.x || DTid.y >= ScreenDim.y)
		return;

	float result = 0.f;

	result += output[OutputIndex][DTid.xy + uint2(-1,  1)] * 1.f;
	result += output[OutputIndex][DTid.xy + uint2( 0,  1)] * 2.f;
	result += output[OutputIndex][DTid.xy + uint2( 1,  1)] * 1.f;

	result += output[OutputIndex][DTid.xy + uint2(-1,  0)] * 2.f;
	result += output[OutputIndex][DTid.xy + uint2( 0,  0)] * 4.f;
	result += output[OutputIndex][DTid.xy + uint2( 1,  0)] * 2.f;

	result += output[OutputIndex][DTid.xy + uint2(-1, -1)] * 1.f;
	result += output[OutputIndex][DTid.xy + uint2( 0, -1)] * 2.f;
	result += output[OutputIndex][DTid.xy + uint2( 1, -1)] * 1.f;

	scratch[DTid.xy] = result / 16.f;
}
