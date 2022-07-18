#pragma once

#include <memory>
#include <vector>

#include "DirectX/Resource.h"

#include "Mathematics/Types.h"

#include "Sampler/ISampler.h"

#include "ILightTracingComponent.h"

#include "RasterShading.h"
#include "RayTracingAOShading.h"
#include "SingleIOComputeShader.h"
#include "Camera.h"

namespace candela::renderer
{
	class LTRasterGuidedShading
		: public ILightTracingComponent
	{
	public:
		LTRasterGuidedShading(sampler::ISampler* sampler, bool storePerLightCDF);

		struct alignas(16) ConstBuff
		{
			DirectX::XMVECTOR plane; // x, y and z (distance from point to plane)
			mathematics::UVector2 lightCamDim;
			std::uint32_t lightIndex;
			float lightCamPdf;
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

		// 
		void setFilterSize(std::uint32_t filterSize);
		std::uint32_t getFilterSize() const;
		void setDistanceMetricMode(std::uint32_t mode);
		std::uint32_t getDistanceMetricMode() const;

	private:
		void generateCDF(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, std::uint32_t lightIndex);
		void regenerateCDFs(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex);

		sampler::ISampler* sampler;

		RendererResources *rendererResources;
		wrl::ComPtr<ID3D12Resource> constantBuffer;

		mathematics::UVector2 cdfSize;
		directx::Resource* cumulativeDistributionTexture;
		std::vector<directx::Resource*> resources;
		std::vector<directx::Resource *> cdfs;
		std::unique_ptr<Camera> lightCamera;

		RasterShading rasterShader;
		RayTracingAOShading rtaoShading;
		DistanceComputeShader distanceComputerShader;
		FilterComputeShader guassianComputerShader;
		PrefixSumComputeShader prefixSumComputeShader;
		NormalisationComputeShader normalisationComputeShader;
		NormalisationPass2ComputeShader normalisationPass2ComputeShader;

		bool storePerLightCDF;
		bool regenerateCDFFlag;
	};
}