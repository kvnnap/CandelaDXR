#pragma once

#include <string>
#include <memory>

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

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList);

		void setInputTexture(directx::Resource* inputTexture);
		void setInputTexture(const std::string& inputTextureName);
		void setOutputTexture(directx::Resource* outputTexture);
		void setAdditionalConstantBuffer(const void* p_cbData, std::size_t p_cbSize);

		void compute(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList);

		static constexpr auto ThreadGroupDim = 8u;
	protected:
		virtual mathematics::UVector2 getLaunchDimensions(const mathematics::UVector2& originalDimensions) const;
		virtual void addAdditionalResources(directx::RootSignatureManager* rsm, const std::string& rangeName);
		virtual void bindAdditionalResources(UINT baseIndex);
		virtual void dispatch(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList);
	//private:
		const std::string shaderPath;
		const bool launchAsFlatArray;

		directx::Resource *inputTexture;
		directx::Resource *outputTexture;
		directx::ResourceManager *resourceManager;
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
}