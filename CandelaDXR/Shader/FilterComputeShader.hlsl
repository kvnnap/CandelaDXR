cbuffer CB1 : register(b0)
{
	uint2 ScreenDim;
	uint InputIndex;
	uint OutputIndex;
}

cbuffer CB2 : register(b1)
{
	uint PassNumber;
	uint FilterSize; // Linear Size
}

Texture2D<float> input[] : register(t0);
RWTexture2D<float> output[] : register(u0);

// Set in subclass
RWTexture2D<float> scratch : register(u0, space1);
Buffer<float> filterCoeff : register(t0, space1);


// 64 threads per group should be optimal on both NVIDIA and AMD
// i.e. 2 warps per thread group or 1 depending on NVIDIA/AMD
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= ScreenDim.x || DTid.y >= ScreenDim.y)
		return;

	const uint halfSize = FilterSize >> 1;
	float result = 0.f;

	if (PassNumber == 0)
	{
		// Vertical first
		for (uint i = 0; i < FilterSize; ++i)
			result += output[OutputIndex][DTid.xy + uint2(i - halfSize, 0)] * filterCoeff[i];
		scratch[DTid.xy] = result;
	}
	else if(PassNumber == 1)
	{
		// Horizontal
		for (uint i = 0; i < FilterSize; ++i)
			result += scratch[DTid.xy + uint2(0, i - halfSize)] * filterCoeff[i];
		output[OutputIndex][DTid.xy] = result;
	}
}
