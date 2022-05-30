#pragma once

#include <memory>

#include "DirectX/Resource.h"

#include "Mathematics/Types.h"

#include "Sampler/ISampler.h"

#include "ILightTracingComponent.h"

#include "RasterShading.h"
#include "SingleIOComputeShader.h"
#include "Camera.h"

namespace candela::renderer
{
	class LTRasterGuidedShading
		: public ILightTracingComponent
	{
	public:
		LTRasterGuidedShading(sampler::ISampler* sampler);

		struct alignas(16) ConstBuff
		{
			DirectX::XMVECTOR u, v, w;
			DirectX::XMVECTOR position;
			DirectX::XMVECTOR direction;
			DirectX::XMVECTOR plane; // x, y and z (distance from point to plane)

			mathematics::UVector2 lightCamDim;
			std::uint32_t lightIndex;
		} constBuffer;

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
		sampler::ISampler* sampler;

		RendererResources *rendererResources;
		wrl::ComPtr<ID3D12Resource> constantBuffer;

		mathematics::UVector2 cdfSize;
		directx::Resource *cumulativeDistributionTexture;
		std::unique_ptr<Camera> lightCamera;

		RasterShading rasterShader;
		DistanceComputeShader distanceComputerShader;
		PrefixSumComputeShader prefixSumComputeShader;
		NormalisationComputeShader normalisationComputeShader;
		NormalisationPass2ComputeShader normalisationPass2ComputeShader;

	};
}