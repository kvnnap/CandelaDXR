#include "TimeStampQuery.h"
#include "CommandQueue.h"

#include "DxUtil.h"
#include "d3dx12.h"

using std::vector;

using candela::directx::ResourceManager;
using candela::directx::DXCommandList;
using candela::directx::DXCommandQueue;
using candela::directx::TimeStampQuery;
using candela::directx::DXUtil;
using candela::directx::ProfileItem;

void TimeStampQuery::init(DXDevice pDevice, ResourceManager *resourceManager, DXCommandQueue& commandQueue, DXCommandList pCommandList)
{
	queryHeap = DXUtil::createQueryHeap(pDevice, numTimeStamps);
	queryResource = &resourceManager->createResource(D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, 8u * numTimeStamps, 1U, DXGI_FORMAT_UNKNOWN, false, "", D3D12_HEAP_TYPE_READBACK);
	queryResource->setName("Query Resource");
	queryResource->transistionBarrier(pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
	UINT64 freq{};
	commandQueue->getCommandQueue()->GetTimestampFrequency(&freq);
	frequency = 1000. / freq;
}

void TimeStampQuery::addTimeStampQuery(DXCommandList pCommandList, const std::string& profName)
{
	if (profNames.size() >= numTimeStamps)
		return;
	pCommandList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, static_cast<UINT>(profNames.size()));
	profNames.push_back(profName);
}

// Load the previous resolve
const vector<ProfileItem>& TimeStampQuery::load()
{
	profItems.clear();
	// Read previous query
	const DXResource& qRes = *queryResource;
	UINT64* timesnaps{};
	qRes->Map(0u, nullptr, reinterpret_cast<void**>(&timesnaps));
	for (std::size_t i = 1; i < committedProfNames.size(); ++i)
	{
		profItems.emplace_back(ProfileItem{
			.ProfileName = committedProfNames[i],
			.TimeMs = static_cast<float>((timesnaps[i] - timesnaps[i - 1]) * frequency)
		});
	}
	const auto range = CD3DX12_RANGE(0, 0);
	qRes->Unmap(0u, &range);
	committedProfNames.clear();

	return profItems;
}

const vector<ProfileItem>& TimeStampQuery::getLoadedItems() const
{
	return profItems;
}

void TimeStampQuery::resolve(DXCommandList pCommandList)
{
	pCommandList->ResolveQueryData(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0u, static_cast<UINT>(profNames.size()), *queryResource, 0u);
	committedProfNames = std::move(profNames);
	profNames.clear();
}
