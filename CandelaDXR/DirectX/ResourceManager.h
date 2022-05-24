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

	using NamedResType = std::map<std::string, Resource*>;

	class ResourceManager
	{
	public:

		ResourceManager(DXDevice& device);
		Resource& createResource(D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags, UINT width, UINT height = 1, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, bool resizeOnResize = false, std::string globalName = "", D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT, D3D12_CLEAR_VALUE* clearValue = nullptr);
		Resource& addExistingResource(DXResource resource, D3D12_RESOURCE_STATES resourceState, std::string globalName = "");

		void resize(UINT width, UINT height);

		Resource* getNamedResource(const std::string& resourceName);
		const NamedResType& getNamedResources() const;

		// Temporary buffers
		void clearTemporaryBuffers(UINT index);
		DXResource& getTempResource(UINT currentBackBufferIndex);
		void setTempBufferSlots(UINT numOfSlots);

	private:
		void addToNamed(Resource *resource, const std::string& globalName);

		DXDevice& device;
		std::vector<std::unique_ptr<ResourceItem>> resources;
		NamedResType namedResources;

		// Temporary Buffers
		std::vector<std::vector<wrl::ComPtr<ID3D12Resource>>> tempBuffers;
	};
}