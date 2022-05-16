#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>

#include "Resource.h"

namespace candela::directx
{
	struct ResourceItem
	{
		ResourceItem(Resource resource) : resource(resource) {}
		Resource resource;
		bool resizeOnResize = false;
	};

	class ResourceManager
	{
	public:
		ResourceManager(DXDevice& device);
		Resource& createResource(D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags, UINT width, UINT height = 1, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, bool resizeOnResize = false, std::string globalName = "", D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT);
		//void createNamedResource();

		void resize(UINT width, UINT height);
		void clearTemporaryBuffers(UINT index);
		DXResource& getTempResource(UINT currentBackBufferIndex);
		void setTempBufferSlots(UINT numOfSlots);

	private:
		DXDevice& device;
		std::vector<std::unique_ptr<ResourceItem>> resources;
		std::map<std::string, Resource*> namedResources;

		std::vector<std::vector<wrl::ComPtr<ID3D12Resource>>> tempBuffers;
		
	};
}