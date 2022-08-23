#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <string>

#include "DirectX/Types.h"
#include "DirectX/DXUtil.h"
#include "DirectX/CommandQueue.h"
#include "DirectX/RootSignatureManager.h"
#include "DirectX/ShadingTable.h"
#include "DirectX/Resource.h"
#include "Scene/Scene.h"
#include "Sampler/UniformSampler.h"

#include "Mathematics/Types.h"

#include "Camera.h"

#include "Drawable.h"

class NrdIntegration;

namespace nri
{
	struct Device;
}

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	struct NriInterface;


	class DenoiserShading
		: public Drawable
	{
	public:
		DenoiserShading();

		~DenoiserShading();

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
		void accept(IVisitor* visitor) override;

	private:
		// Common renderer resources
		RendererResources* rendererResources;
		std::unique_ptr<NrdIntegration> NRD;
		nri::Device *nriDevice;
		std::unique_ptr<NriInterface> NRI;
	};
}