cbuffer CB1 : register(b0)
{
	uint2 ScreenDim;
}

Texture2D<float> input : register(t0);
RWTexture2D<float> output : register(u0);

// 64 threads per group should be optimal on both NVIDIA and AMD
// i.e. 2 warps per thread group or 1 depending on NVIDIA/AMD
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	const uint2 lastPixelIndex = ScreenDim - 1;
	if (any(DTid.xy > lastPixelIndex) || all(DTid.xy == lastPixelIndex))
		return;

	output[DTid.xy] /= output[lastPixelIndex];
}