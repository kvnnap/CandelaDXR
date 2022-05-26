cbuffer CB1 : register(b0)
{
	uint2 ScreenDim;
}

Texture2D<float> input : register(t0);
RWTexture2D<float> output : register(u0);

uint2 getIndex(uint id)
{
	return uint2(id % ScreenDim.x, id / ScreenDim.x);
}

// 64 threads per group should be optimal on both NVIDIA and AMD
// i.e. 2 warps per thread group or 1 depending on NVIDIA/AMD
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float lastElement = output[getIndex(ScreenDim.x * ScreenDim.y - 1)];
	if (DTid.x >= ScreenDim.x || DTid.y >= ScreenDim.y)
		return;

	output[DTid.xy] /= lastElement;
}