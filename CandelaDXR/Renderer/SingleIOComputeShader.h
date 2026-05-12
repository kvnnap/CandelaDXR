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

		void init(RendererResources* rendererResources, directx::DXCommandList& pCurrentCommandList, std::vector<directx::Resource*>* res, std::uint32_t numOutputs = 1u, std::uint32_t numInputs = 1u);

		void setInputTexture(std::uint32_t inputIndex);
		void setOutputTexture(std::uint32_t outputIndex);
		void setAdditionalConstantBuffer(const void* p_cbData, std::size_t p_cbSize);

		void compute(directx::DXCommandList currentCommandList);

		// Call if resources change size
		void bindResources();

		static constexpr auto ThreadGroupDim = 8u;
	protected:
		virtual mathematics::UVector2 getLaunchDimensions(const mathematics::UVector2& originalDimensions) const;
		virtual void addAdditionalResources(directx::RootSignatureManager* rsm, const std::string& rangeName);
		virtual void bindAdditionalResources(UINT baseIndex);
		virtual void updateData(directx::DXCommandList currentCommandList);
		virtual void dispatch(directx::DXCommandList currentCommandList);
		virtual void initComponent(RendererResources* rendererResources, directx::DXCommandList& pCurrentCommandList);
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
		DistanceComputeShader();
		void addAdditionalResources(directx::RootSignatureManager* rsm, const std::string& rangeName) override;
		void bindAdditionalResources(UINT baseIndex) override;
		void updateData(directx::DXCommandList currentCommandList) override;
		void dispatch(directx::DXCommandList currentCommandList) override;
		void setMode(std::uint32_t mode);
		std::uint32_t getMode() const;

		struct alignas(256) DistConstBuff
		{
			DirectX::XMVECTOR position;
			DirectX::XMVECTOR plane;
			DirectX::XMVECTOR planeU;
			DirectX::XMVECTOR planeV;
			DirectX::XMVECTOR camPosition;
			DirectX::XMVECTOR camUnitDir;
			std::uint32_t mode;
			std::uint32_t orthographic;
			std::uint32_t singlePointSource;
			std::uint32_t lightType;
		} distConstBuffer;
	private:
		directx::Resource* faceIndexResource;
		directx::Resource* normalResource;
		directx::Resource* constBufferResource;
		directx::Resource* cdfMaskResource;
	};

	class FilterComputeShader
		: public SingleIOComputeShader
	{
	public:
		FilterComputeShader();
		void addAdditionalResources(directx::RootSignatureManager* rsm, const std::string& rangeName) override;
		void bindAdditionalResources(UINT baseIndex) override;
		void updateData(directx::DXCommandList currentCommandList) override;
		void dispatch(directx::DXCommandList currentCommandList) override;
		void initComponent(RendererResources* rendererResources, directx::DXCommandList& pCurrentCommandList) override;

		void setDxgiFormat(DXGI_FORMAT format);
		void setFiltersize(std::uint32_t filterSize);
		std::uint32_t getFiltersize() const;
		
		static constexpr auto InitialSize = 17u;
		static constexpr auto MaxSize = 1023u;
		static constexpr auto StdDeviations = 3.f;

	private:
		directx::Resource* cbvResource;
		directx::Resource* scratchResource;
		DXGI_FORMAT scratchResFormat;
		std::vector<float> linearFilterCoeff;
		bool changed;
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
		void dispatch(directx::DXCommandList currentCommandList) override;
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

		void dispatch(directx::DXCommandList currentCommandList) override
		{
			currentCommandList->Dispatch(1u, 1u, 1u);
		}
	};
}