cbuffer CB1 : register(b0)
{
	uint2 ScreenDim;
	uint InputIndex;
	uint OutputIndex;
}

cbuffer CB1 : register(b1)
{
	float3 Position;
	float3 UnitDirection;
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
	const float3 dir = inData.xyz - Position;
	const float lenDir = length(dir);

	// Experimental emptiness weighting using radial distance
	const float2 halfDim = 0.5f * ScreenDim;
	const float maxRad = length(halfDim);
	const float rad = length(DTid.xy - halfDim);

	float result = inData.w == 1.f ? /*lenDir * */dot(dir/* * (1.f / lenDir)*/, UnitDirection) : 0.015625f * cos(1.5625f * (rad / maxRad));
	output[OutputIndex][DTid.xy] = result;
}
