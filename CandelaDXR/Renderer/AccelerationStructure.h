#pragma once

#include <wrl/client.h>
#include <vector>

#include "IDrawable.h"
#include "IResource.h"

#include "DirectX/DxUtil.h"
#include "RendererResources.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class AccelerationStructure
		: public IResource
	{
	public:
		AccelerationStructure();

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList> &pCurrentCommandList) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void buildTlas(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList, wrl::ComPtr<ID3D12Resource>& tempResource);

		D3D12_GPU_VIRTUAL_ADDRESS getTopLayerBufferAddress() const;
	private:
		std::vector<directx::DXUtil::AccelerationStructureBuffers> blasBuffers;
		directx::DXUtil::AccelerationStructureBuffers tlasBuffers;
		std::vector<directx::DXUtil::TopLevelAccelerationData> tlasInstanceData;
		std::vector<wrl::ComPtr<ID3D12Resource>> tlasTempBuffer;

		RendererResources* rendererResources;
	};
}
