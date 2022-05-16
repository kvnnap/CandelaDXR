#include "ResourceManager.h"

using std::unique_ptr;
using std::make_unique;

using candela::directx::Resource;
using candela::directx::ResourceItem;
using candela::directx::ResourceManager;
using candela::directx::DXResource;

ResourceManager::ResourceManager(DXDevice& device)
	: device (device)
{
}

Resource& ResourceManager::createResource(D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags, UINT width, UINT height, DXGI_FORMAT format, bool resizeOnResize, std::string globalName, D3D12_HEAP_TYPE heapType)
{
	unique_ptr<ResourceItem> res;
	if (format == DXGI_FORMAT_UNKNOWN) // Assume buffer for unknown format
		res = make_unique<ResourceItem>(Resource::createCommittedResource(device, width * height, resourceState, resourceFlags, heapType));
	else
		res = make_unique<ResourceItem>(Resource::createTextureCommittedResource(device, width, height, resourceState, format, resourceFlags, heapType));

	res->resizeOnResize = resizeOnResize;
	if (!globalName.empty())
		namedResources[globalName] = &res->resource;
	return resources.emplace_back(std::move(res))->resource;
}

void ResourceManager::resize(UINT width, UINT height)
{
	for (auto& res : resources)
	{
		if (!res->resizeOnResize)
			continue;
		res->resource.resize(width, height);
	}
}

// Temporary Buffers
void ResourceManager::clearTemporaryBuffers(UINT index)
{
	tempBuffers[index].clear();
}

DXResource& ResourceManager::getTempResource(UINT currentBackBufferIndex)
{
	return tempBuffers[currentBackBufferIndex].emplace_back();
}

void ResourceManager::setTempBufferSlots(UINT numOfSlots)
{
	tempBuffers.resize(numOfSlots);
}
