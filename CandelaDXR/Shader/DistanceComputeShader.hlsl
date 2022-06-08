cbuffer CB1 : register(b0)
{
	uint2 ScreenDim;
	uint InputIndex;
	uint OutputIndex;
}

cbuffer CB1 : register(b1)
{
	float3 Position;
}

Texture2D<float4> input[] : register(t0);
RWTexture2D<float> output[] : register(u0);

// 64 threads per group should be optimal on both NVIDIA and AMD
// i.e. 2 warps per thread group or 1 depending on NVIDIA/AMD
[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if (DTid.x >= ScreenDim.x || DTid.y >= ScreenDim.y)
		return;
	const float4 inData = input[InputIndex][DTid.xy];
	output[OutputIndex][DTid.xy] = inData.w == 1.f ? length(inData.xyz - Position) : 0.125f;
}
