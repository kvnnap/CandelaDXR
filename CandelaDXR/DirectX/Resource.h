#pragma once

#include "Window/WindowsDef.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include <vector>

#include <DirectXMath.h>

#include "DirectX/Types.h"
#include "Mathematics/Types.h"

namespace candela::directx
{
	struct ResourceData
	{
		std::wstring Name;
		UINT64 Width;
		UINT Height;
		std::vector<DirectX::XMFLOAT4> data;
	};

	class Resource 
	{
	public:
		Resource(DXResource resource, D3D12_RESOURCE_STATES state);

		// Use when external api's modify the state - DOES NOT emit a barrier
		void rewriteState(D3D12_RESOURCE_STATES currentState);
		void transistionBarrier(DXCommandList& commandList, D3D12_RESOURCE_STATES state);
		void transitionToPrevBarrier(DXCommandList &commandList);
		void uavBarrier(DXCommandList &commandList);
		D3D12_RESOURCE_STATES getState() const;
		operator DXResource& ();
		operator const DXResource&() const;
		operator ID3D12Resource*();

		ResourceData read(DXCommandQueue& commandQueue);
		void write(DXCommandList& commandList, DXResource& tempResource, const void * ptData);
		void resize(std::uint32_t width, std::uint32_t height);

		void createShaderResourceView(D3D12_CPU_DESCRIPTOR_HANDLE heapSrvCpuDesc);

		float getAspectRatio();
		mathematics::UVector2 getDimensions();
		void setResource(DXResource resource, D3D12_RESOURCE_STATES state);

		void setName(const std::wstring& name);
		void setName(const std::string& name);
		std::wstring getName() const;

		static Resource createTextureCommittedResource(DXDevice& device, UINT64 width, UINT height, D3D12_RESOURCE_STATES resourceState, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT, D3D12_CLEAR_VALUE* clearValue = nullptr);
		static Resource createCommittedResource(DXDevice &device, UINT64 size, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT, D3D12_CLEAR_VALUE* clearValue = nullptr);
	private:
		DXResource resource;
		D3D12_RESOURCE_STATES state, prevState;
	};
}
