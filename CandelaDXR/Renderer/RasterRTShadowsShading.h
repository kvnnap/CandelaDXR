#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <string>

#include "DirectX/DXUtil.h"
#include "DirectX/Types.h"
#include "DirectX/CommandQueue.h"
#include "DirectX/RootSignatureManager.h"
#include "DirectX/ShadingTable.h"
#include "DirectX/Resource.h"
#include "Scene/Scene.h"
#include "Sampler/ISampler.h"

#include "Mathematics/Types.h"

#include "Camera.h"

#include "IDrawable.h"

#include "RasterShading.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class RasterRTShadowsShading
		: public IDrawable
	{
	public:
		RasterRTShadowsShading(std::unique_ptr<sampler::ISampler> sampler);

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
		void accept(IVisitor* visitor) override;

		std::uint32_t getLightType() const;
		void setLightType(std::uint32_t);
	private:
		void buildPipeline();
		void createShaderResources();
		void createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList, std::int32_t currentBackBufferIndex = -1);

		// Raster Shader
		RasterShading rasterShader;

		// Common renderer resources
		RendererResources* rendererResources;

		// Path tracer descriptor stuff
		std::vector<wrl::ComPtr<ID3D12DescriptorHeap>> descriptorHeaps;
		wrl::ComPtr<ID3D12RootSignature> globalEmptyRootSignature;
		wrl::ComPtr<ID3D12StateObject> stateObject;

		// Path tracing shader resources
		struct alignas(16) ConstBuff
		{
			std::uint32_t seeds[2];
			mathematics::UVector2 winDimensions;
			std::uint32_t numLights;
			std::uint32_t frameNumber;
			std::uint32_t lightType;
		} constBuffer;

		std::unique_ptr<directx::Resource> radianceTexture;
		wrl::ComPtr<ID3D12Resource> constantBuffer;
		std::unique_ptr<sampler::ISampler> sampler;
		bool clear;

		// My helpers
		std::vector<std::unique_ptr<directx::ShadingTable>> shadingTables;
		std::shared_ptr<directx::RootSignatureManager> rootSignatureManager;
	};
}