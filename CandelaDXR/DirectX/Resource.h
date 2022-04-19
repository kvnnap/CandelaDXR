#pragma once

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

namespace candela::directx
{
	namespace wrl = Microsoft::WRL;

	using DXDevice = wrl::ComPtr<ID3D12Device>;
	using DXResource = wrl::ComPtr<ID3D12Resource>;
	using DXCommandList = wrl::ComPtr<ID3D12GraphicsCommandList>;

	class Resource {
	public:
		Resource(DXResource resource, D3D12_RESOURCE_STATES state);

		void transistionBarrier(DXCommandList &commandList, D3D12_RESOURCE_STATES state);
		void uavBarrier(DXCommandList &commandList);
		operator DXResource();

		void setName(const std::wstring& name);
		std::wstring getName() const;

		static Resource createTextureCommittedResource(DXDevice& device, UINT64 width, UINT height, D3D12_RESOURCE_STATES resourceState, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT);
		static Resource createCommittedResource(DXDevice &device, UINT64 size, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT);
	private:
		DXResource resource;
		D3D12_RESOURCE_STATES state;
	};
}
