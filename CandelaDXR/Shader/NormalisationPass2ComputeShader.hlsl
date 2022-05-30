cbuffer CB1 : register(b0)
{
	uint2 ScreenDim;
}

Texture2D<float> input : register(t0);
RWTexture2D<float> output : register(u0);

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint2 lastPixelIndex = ScreenDim - 1;
	if (output[lastPixelIndex] != 0.f)
		output[lastPixelIndex] = 1.f;
}
