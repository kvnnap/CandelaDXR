#pragma once

#include <string>
#include <memory>
#include <vector>

#include "DirectX/Resource.h"
#include "DirectX/ResourceManager.h"
#include "DirectX/RootSignatureManager.h"
#include "DirectX/ShadingTable.h"

#include "Mathematics/Types.h"

#include "RendererResources.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class SingleIOComputeShader
	{
	public:
		SingleIOComputeShader(const std::string& shaderPath, bool launchAsFlatArray = false);
		virtual ~SingleIOComputeShader() = default;

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, std::vector<directx::Resource*>* res, std::uint32_t numOutputs = 1u, std::uint32_t numInputs = 1u);

		void setInputTexture(std::uint32_t inputIndex);
		void setOutputTexture(std::uint32_t outputIndex);
		void setAdditionalConstantBuffer(const void* p_cbData, std::size_t p_cbSize);

		void compute(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList);

		static constexpr auto ThreadGroupDim = 8u;
	protected:
		virtual mathematics::UVector2 getLaunchDimensions(const mathematics::UVector2& originalDimensions) const;
		virtual void addAdditionalResources(directx::RootSignatureManager* rsm, const std::string& rangeName);
		virtual void bindAdditionalResources(UINT baseIndex);
		virtual void dispatch(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList);
		virtual void initComponent(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList);
	//private:
		RendererResources* rendererResources;
		const std::string shaderPath;
		const bool launchAsFlatArray;
		std::uint32_t numInputs;
		std::uint32_t numOutputs;
		directx::ResourceManager* resourceManager;

		std::vector<directx::Resource*>* resources;
		std::uint32_t inputTextureIndex;
		std::uint32_t outputTextureIndex;
		directx::Resource* inputTexture;
		directx::Resource* outputTexture;
		ID3D12Device* pDevice;
		const void* cbData;
		std::size_t cbSize;

		// Compute shader
		std::unique_ptr<directx::DescriptorHeap> descHeapManager;
		wrl::ComPtr<ID3D12RootSignature> computeRootSignature;
		wrl::ComPtr<ID3D12PipelineState> computePipelineState;
	};

	class DistanceComputeShader
		: public SingleIOComputeShader
	{
	public:
		DistanceComputeShader() : SingleIOComputeShader("./Shaders/DistanceComputeShader.cso") {}
	};

	class FilterComputeShader
		: public SingleIOComputeShader
	{
	public:
		FilterComputeShader(std::uint32_t filterSize);
		void addAdditionalResources(directx::RootSignatureManager* rsm, const std::string& rangeName) override;
		void bindAdditionalResources(UINT baseIndex) override;
		void dispatch(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList) override;
		void initComponent(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList) override;
	private:
		directx::Resource* cbvResource;
		directx::Resource* scratchResource;
		std::vector<float> linearFilterCoeff;
	};

	class PrefixSumComputeShader
		: public SingleIOComputeShader
	{
	public:
		PrefixSumComputeShader();

		static constexpr auto ThreadGroupDim = 1024u;
	protected:
		mathematics::UVector2 getLaunchDimensions(const mathematics::UVector2& dim) const override;
		void addAdditionalResources(directx::RootSignatureManager* rsm, const std::string& rangeName) override;
		void bindAdditionalResources(UINT baseIndex) override;
		void dispatch(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList) override;
	private:
		directx::Resource* scratchResource;
	};

	class NormalisationComputeShader
		: public SingleIOComputeShader
	{
	public:
		NormalisationComputeShader() : SingleIOComputeShader("./Shaders/NormalisationComputeShader.cso") {}
	};

	class NormalisationPass2ComputeShader
		: public SingleIOComputeShader
	{
	public:
		NormalisationPass2ComputeShader() : SingleIOComputeShader("./Shaders/NormalisationPass2ComputeShader.cso") {}

		void dispatch(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList) override
		{
			currentCommandList->Dispatch(1u, 1u, 1u);
		}
	};
}