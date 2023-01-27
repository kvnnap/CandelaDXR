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
#include "ILightTracingComponent.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class LightTracingShading
		: public Drawable
	{
	public:
		LightTracingShading(std::unique_ptr<sampler::ISampler> sampler, mathematics::UVector2 lightSamples);

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList> &pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
		void accept(IVisitor* visitor) override;
		std::uint32_t getBufferUsage() const override;

		const mathematics::UVector2& getLightSamples() const;
		void setLightSamples(const mathematics::UVector2& lightSamples);

		void setCurrentShaderIndex(std::uint32_t currentShaderIndex);
		std::uint32_t getCurrentShaderIndex() const;

		std::uint32_t getPathFilter() const;
		void setPathFilter(std::uint32_t pathFilter);

		std::uint32_t getMinBounces() const;
		void setMinBounces(std::uint32_t minBounces);
		std::uint32_t getMaxBounces() const;
		void setMaxBounces(std::uint32_t maxBounces);
		std::uint32_t getSeperateCaustics() const;
		void seperateCaustics(std::uint32_t sepCaustics);

		struct LTShaderInfo
		{
			std::string* shaderPath;
			ILightTracingComponent* component;
		};

		std::vector<LTShaderInfo> getLTShaderInfo();

	private:
		void buildPipeline();
		void createShaderResources(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList);
		void createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList> &commandList);
		
		void generateIrrToRadTexture(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList, wrl::ComPtr<ID3D12Resource>& tempResource);

		mathematics::Vector2 toSensorSpace(std::uint32_t x, std::uint32_t y) const;
		float cosIntegral(std::uint32_t x, std::uint32_t y) const;

		// Common renderer resources
		RendererResources* rendererResources;
		
		// Shader paths
		struct LTShader
		{
			std::string shaderPath;
			std::unique_ptr<ILightTracingComponent> component;
			std::unique_ptr<directx::ShadingTable> shadingTable;
			std::shared_ptr<directx::DescriptorHeap> descHeapManager;
			std::shared_ptr<directx::RootSignatureManager> rootSignatureManager;
			wrl::ComPtr<ID3D12StateObject> stateObject;
		};
		std::vector<LTShader> ltShaders;

		// Light tracer descriptor stuff
		wrl::ComPtr<ID3D12RootSignature> globalEmptyRootSignature;

		// Light tracing shader resources
		struct alignas(16) ConstBuff
		{
			DirectX::XMVECTOR u, v, w;
			DirectX::XMVECTOR position;
			DirectX::XMVECTOR direction;
			DirectX::XMVECTOR plane; // x, y and z (distance from point to plane)
			std::uint32_t seeds[2];
			mathematics::UVector2 winDimensions;
			std::uint32_t numLights;
			std::uint32_t numTotalLights;
			std::uint32_t frameNumber;
			std::uint32_t pathFilter;
			std::uint32_t minBounces;
			std::uint32_t maxBounces;
			std::uint32_t seperateCaustics;
		} constBuffer;
		mathematics::UVector2 lightSamples;

		wrl::ComPtr<ID3D12Resource> constantBuffer;
		directx::Resource* irradianceDataStructure;
		directx::Resource* irrToRad;
		directx::Resource* rayHitT;
		directx::Resource* irradianceCaustics;
		directx::Resource* outputCaustics;

		directx::Resource* irradianceTexture;
		std::unique_ptr<sampler::ISampler> sampler;
		bool clear;

		// Compute shader
		wrl::ComPtr<ID3D12DescriptorHeap> computeDescriptorHeap;
		wrl::ComPtr<ID3D12RootSignature> computeRootSignature;
		wrl::ComPtr<ID3D12PipelineState> computePipelineState;

		// Component for pre-pass
		//std::vector<std::unique_ptr<ILightTracingComponent>> ltComponents;

		// My helpers
		std::shared_ptr<directx::RootSignatureManager> computeRSM;
		std::uint32_t currentShader;
	};
}