RWTexture2D<float4> gOutput : register(u0);

// 64 threads per group should be optimal on both NVIDIA and AMD
// i.e. 2 warps per thread group or 1 depending on NVIDIA/AMD
// 32 threads per group might be better on NVIDIA.. no. It seems one SM can dispatch
// more than 1 warp concurrently..? Not sure.
[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	uint2 launchIndex = uint2(0, 0);
	gOutput[launchIndex] = float4(1.f, 0.f, 0.f, 1.f);
}