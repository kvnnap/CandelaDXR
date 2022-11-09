#include "ResourceManager.h"

using std::unique_ptr;
using std::make_unique;
using std::string;

using candela::directx::Resource;
using candela::directx::ResourceItem;
using candela::directx::NamedResType;
using candela::directx::ResourceManager;
using candela::directx::DXResource;

ResourceManager::ResourceManager(DXDevice& device)
	: device (device)
{
}

Resource& ResourceManager::createResource(D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags, UINT width, UINT height, DXGI_FORMAT format, bool resizeOnResize, std::string globalName, D3D12_HEAP_TYPE heapType, D3D12_CLEAR_VALUE *clearValue)
{
	unique_ptr<ResourceItem> res;
	if (format == DXGI_FORMAT_UNKNOWN) // Assume buffer for unknown format
		res = make_unique<ResourceItem>(Resource::createCommittedResource(device, width * height, resourceState, resourceFlags, heapType, clearValue));
	else
		res = make_unique<ResourceItem>(Resource::createTextureCommittedResource(device, width, height, resourceState, format, resourceFlags, heapType, clearValue));

	res->resizeOnResize = resizeOnResize;
	addToNamed(&res->resource, globalName);
	return resources.emplace_back(std::move(res))->resource;
}

Resource& ResourceManager::createResourceIfNotExists(D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags, UINT width, UINT height, DXGI_FORMAT format, bool resizeOnResize, std::string globalName, D3D12_HEAP_TYPE heapType, D3D12_CLEAR_VALUE* clearValue)
{
	return namedResourceExists(globalName) ?
		*getNamedResource(globalName) :
		createResource(resourceState, resourceFlags, width, height, format, resizeOnResize, globalName, heapType, clearValue);
}

Resource& ResourceManager::addExistingResource(DXResource resource, D3D12_RESOURCE_STATES resourceState, std::string globalName)
{
	auto res = make_unique<ResourceItem>(Resource(resource, resourceState));
	addToNamed(&res->resource, globalName);
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

Resource* ResourceManager::getNamedResource(const string& resourceName)
{
	return namedResources.at(resourceName);
}

bool ResourceManager::namedResourceExists(const string& resourceName) const
{
	return namedResources.contains(resourceName);
}

const NamedResType& ResourceManager::getNamedResources() const
{
	return namedResources;
}

void ResourceManager::addToNamed(Resource* resource, const std::string& globalName)
{
	if (globalName.empty())
		return;
	namedResources.emplace(globalName, resource);
	if (resource->getName().length() == 0)
		resource->setName(globalName);
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
