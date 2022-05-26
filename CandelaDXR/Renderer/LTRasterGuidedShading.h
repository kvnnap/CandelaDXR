#pragma once

#include "DirectX/Resource.h"

#include "Mathematics/Types.h"

#include "ILightTracingComponent.h"

#include "RasterShading.h"
#include "SingleIOComputeShader.h"

namespace candela::renderer
{
	class LTRasterGuidedShading
		: public ILightTracingComponent
	{
	public:
		LTRasterGuidedShading();

		// IDrawable
		virtual void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		virtual void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		virtual void accept(IVisitor* visitor) override;

		// On matrix change
		virtual void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;

		// On window resize
		virtual void onResize() override;

		// Is enabled?
		virtual bool isEnabled() const override;
		virtual void setEnabled(bool p_enabled) override;

		virtual void appendToPipeline(directx::RootSignatureManager* rootSignatureManager) override;
		virtual void appendToShaderTable(directx::ShadingTable* shadingTable) override;
		virtual void appendToDescHeapManager(directx::DescriptorHeap* descriptorHeap) override;
	private:
		RendererResources *rendererResources;

		mathematics::UVector2 cdfSize;
		directx::Resource *cumulativeDistributionTexture;
		RasterShading rasterShader;
		DistanceComputeShader distanceComputerShader;
		PrefixSumComputeShader prefixSumComputeShader;
		NormalisationComputeShader normalisationComputeShader;

	};
}