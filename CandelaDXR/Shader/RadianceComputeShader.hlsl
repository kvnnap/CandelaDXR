RWTexture2D<float4> gOutput : register(u0);

// 64 threads per group should be optimal on both NVIDIA and AMD
// i.e. 2 warps per thread group or 1 depending on NVIDIA/AMD
// 32 threads per group might be better on NVIDIA.. no. It seems one SM can dispatch
// more than 1 warp concurrently..? Not sure.
[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if (DTid.x >= 900 || DTid.y >= 600)
		return;
	const uint resY = (DTid.y / 8) % 3;
	const uint result = ((DTid.x / 8) + resY) % 3;
	float redValue		= result == 0 ? 1.f : 0.f;
	float blueValue		= result == 1 ? 1.f : 0.f;
	float greenValue	= result == 2 ? 1.f : 0.f;
	gOutput[DTid.xy] = float4(redValue, blueValue, greenValue, 1.f);
}