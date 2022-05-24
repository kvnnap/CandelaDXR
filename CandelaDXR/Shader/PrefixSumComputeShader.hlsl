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
[numthreads(64, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	uint idPrev = DTid.x * 2;
	uint id = idPrev + 1;
	/*if (id >= (ScreenDim.x * ScreenDim.y))
		return;*/

	// Copy input to output
	uint2 xyPrev = getIndex(idPrev);
	uint2 xy = getIndex(id);

	// Output is now the working array
	uint maxIter = log2(ScreenDim.x * ScreenDim.y);
	for (uint i = 0; i < maxIter; ++i)
	{
		output[xy] += output[xyPrev];
		
		// Generate next id
		uint d = (1 << i);
		if (((DTid.x / d) % 2) == 0)
			id += d;
		xy = getIndex(id);

		// Generate next idPrev
		uint dPlus1 = (d << 1);
		uint dPlus2 = (d << 2);
		idPrev = (DTid.x / dPlus2) * dPlus2 + dPlus1 - 1;
		xyPrev = getIndex(idPrev);
		
		// Sync
		AllMemoryBarrierWithGroupSync();
	}

	// Divide current pixel by final value
	idPrev = DTid.x * 2;
	id = idPrev + 1;
	xyPrev = getIndex(idPrev);
	xy = getIndex(id);

	float factor = 1.f / output[ScreenDim - 1];
	AllMemoryBarrierWithGroupSync();

	output[xyPrev] *= factor;
	output[xy] *= factor;
}