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

		void init(RendererResources* rendererResources, directx::DXCommandList &pCurrentCommandList) override;
		void onChange(directx::DXCommandList pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void buildTlas(directx::DXCommandList& commandList, directx::DXResource& tempResource);

		D3D12_GPU_VIRTUAL_ADDRESS getTopLayerBufferAddress() const;
	private:
		std::vector<directx::DXUtil::AccelerationStructureBuffers> blasBuffers;
		directx::DXUtil::AccelerationStructureBuffers tlasBuffers;
		std::vector<directx::DXUtil::TopLevelAccelerationData> tlasInstanceData;
		std::vector<directx::DXResource> tlasTempBuffer;

		RendererResources* rendererResources;
	};
}
