cbuffer CB1 : register(b0)
{
	uint2 ScreenDim;
}

cbuffer CB2 : register(b1)
{
	uint PassNumber;
}

Texture2D<float> input : register(t0);
RWTexture2D<float> output : register(u0);
RWStructuredBuffer<float> scratch : register(u1);

uint2 getIndex(uint id)
{
	return uint2(id % ScreenDim.x, id / ScreenDim.x);
}

#define NUM_BANKS 32
#define LOG_NUM_BANKS 5
#define CONFLICT_FREE_OFFSET(n) ((n) >> LOG_NUM_BANKS)
#define CONFLICT_FREE_ID(i) ((i) + CONFLICT_FREE_OFFSET(i))
#define BlOCK_SIZE 1024
#define ARRAY_SIZE (BlOCK_SIZE * 2)

struct Input
{
	uint3 DTid : SV_DispatchThreadID;
	uint3 GTid : SV_GroupThreadID;
	uint3 GroupId : SV_GroupID;
};

// Assign additional space to avoid bank conflicts
groupshared float temp[ARRAY_SIZE + CONFLICT_FREE_OFFSET(ARRAY_SIZE)];
groupshared float lastItem;
groupshared uint groupId;

// BlOCK_SIZE threads per group to handle a list of max ARRAY_SIZE values per group
[numthreads(BlOCK_SIZE, 1, 1)]
void main(const Input input)
{
	uint dIdPrev = input.DTid.x * 2;
	uint dId = dIdPrev + 1;

	// We can assume that out of bounds reads return 0 and 
	// writes do nothing (noop). We do not need manual checking.
	//uint outOfBounds = dId >= ScreenDim.x * ScreenDim.y;

	if (PassNumber == 0 || PassNumber == 1)
	{
		uint totalSize = ScreenDim.x * ScreenDim.y;
		uint gIdPrev = input.GTid.x * 2;
		uint gId = gIdPrev + 1;

		if (PassNumber == 0)
		{
			// Copy input to group shared memory
			temp[CONFLICT_FREE_ID(gIdPrev)] = output[getIndex(dIdPrev)];
			temp[CONFLICT_FREE_ID(gId)] = output[getIndex(dId)];
		}
		else
		{
			// Copy scratch to group shared memory
			temp[CONFLICT_FREE_ID(gIdPrev)] = scratch[dIdPrev];
			temp[CONFLICT_FREE_ID(gId)] = scratch[dId];
			totalSize = totalSize / ARRAY_SIZE + (totalSize % ARRAY_SIZE == 0 ? 0 : 1);
		}

		if (input.GTid.x == 0)
			groupId = input.GroupId.x;

		GroupMemoryBarrierWithGroupSync();

		// Find last item in this block
		uint lastItemIndex = (groupId + 1) * ARRAY_SIZE - 1;
		lastItemIndex = lastItemIndex < totalSize ? ARRAY_SIZE - 1 : (totalSize - 1) % ARRAY_SIZE;
		// The if condition ensures that the same thread that wrote to temp reads the same index
		// And avoids one barrier
		if (input.GTid.x == (lastItemIndex >> 1))
			lastItem = temp[CONFLICT_FREE_ID(lastItemIndex)];

		// Perform Blelloch up-sweep
		uint offset = 1;
		uint d;
		for (d = (lastItemIndex + 1) >> 1; d > 0; d >>= 1)
		{
			GroupMemoryBarrierWithGroupSync();

			if (input.GTid.x < d)
			{
				const uint ai = CONFLICT_FREE_ID(offset * gId - 1);
				const uint bi = CONFLICT_FREE_ID(offset * (gId + 1) - 1);
				temp[bi] += temp[ai];
			}

			offset <<= 1;
		}

		// Clear the last element - no barrier needed before, last active thread was the first one alone
		if (input.GTid.x == 0)
			temp[CONFLICT_FREE_ID(lastItemIndex)] = 0.f;

		// Perform Blelloch down-sweep
		for (d = 1; d < (lastItemIndex + 1); d <<= 1)
		{
			offset >>= 1;
			GroupMemoryBarrierWithGroupSync();

			if (input.GTid.x < d)
			{
				const uint ai = CONFLICT_FREE_ID(offset * gId - 1);
				const uint bi = CONFLICT_FREE_ID(offset * (gId + 1) - 1);

				const float t = temp[ai];
				temp[ai] = temp[bi];
				temp[bi] += t;
			}
		}

		// Left shift and Add lastItem
		GroupMemoryBarrierWithGroupSync();

		float t1 = lastItemIndex == gIdPrev ? temp[CONFLICT_FREE_ID(gIdPrev)] + lastItem : temp[CONFLICT_FREE_ID(gId)];
		float t2 = lastItemIndex == gId ? temp[CONFLICT_FREE_ID(gId)] + lastItem : temp[CONFLICT_FREE_ID(gId + 1)];

		if (PassNumber == 0)
		{
			output[getIndex(dIdPrev)] = t1;
			output[getIndex(dId)] = t2;

			// Populate Scratch
			if (input.GTid.x == (lastItemIndex >> 1))
				scratch[groupId] = lastItemIndex == gIdPrev ? t1 : t2;
		}
		else
		{
			scratch[dIdPrev] = t1;
			scratch[dId] = t2;
		}
	}
	else if (PassNumber == 2)
	{
		// Add scratch to respective arrays - Pass 2 is started with one less block, so we must offset
		output[getIndex(ARRAY_SIZE + dIdPrev)] += scratch[input.GroupId.x];
		output[getIndex(ARRAY_SIZE + dId)] += scratch[input.GroupId.x];
	}
}