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

#include "IDrawable.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class LightTracingShading
		: public IDrawable
	{
	public:
		LightTracingShading(std::unique_ptr<sampler::ISampler> sampler, mathematics::UVector2 lightSamples);

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList> &pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
		void accept(IVisitor* visitor) override;

		const mathematics::UVector2& getLightSamples() const;
		void setLightSamples(const mathematics::UVector2& lightSamples);

		const std::vector<std::string>& getShaderPaths() const;
		void setCurrentShaderIndex(std::uint32_t currentShaderIndex);
		std::uint32_t getCurrentShaderIndex() const;

		void setCausticsRatio(float p_causticsRatio);
		float getCausticsRatio() const;

		std::uint32_t getPathFilter() const;
		void setPathFilter(std::uint32_t pathFilter);

		std::uint32_t getMinBounces() const;
		void setMinBounces(std::uint32_t minBounces);
		std::uint32_t getMaxBounces() const;
		void setMaxBounces(std::uint32_t maxBounces);

	private:
		void buildPipeline();
		void createShaderResources();
		void createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList> &commandList);
		
		void generateIrrToRadTexture(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList, wrl::ComPtr<ID3D12Resource>& tempResource);

		mathematics::Vector2 toSensorSpace(std::uint32_t x, std::uint32_t y) const;
		float cosIntegral(std::uint32_t x, std::uint32_t y) const;

		// Common renderer resources
		RendererResources* rendererResources;
		
		// Shader paths
		std::vector<std::string> shaderPaths;

		// Light tracer descriptor stuff
		wrl::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		wrl::ComPtr<ID3D12RootSignature> globalEmptyRootSignature;
		std::vector<wrl::ComPtr<ID3D12StateObject>> stateObjects;

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
			std::uint32_t numSpeculars;
			std::uint32_t frameNumber;
			float causticsRatio;
			std::uint32_t pathFilter;
			std::uint32_t minBounces;
			std::uint32_t maxBounces;
		} constBuffer;
		mathematics::UVector2 lightSamples;

		wrl::ComPtr<ID3D12Resource> constantBuffer;
		wrl::ComPtr<ID3D12Resource> irradianceDataStructure;
		directx::Resource* irrToRad;

		directx::Resource* irradianceTexture;
		std::unique_ptr<sampler::ISampler> sampler;
		bool clear;

		// Compute shader
		wrl::ComPtr<ID3D12DescriptorHeap> computeDescriptorHeap;
		wrl::ComPtr<ID3D12RootSignature> computeRootSignature;
		wrl::ComPtr<ID3D12PipelineState> computePipelineState;

		// My helpers
		std::vector<std::unique_ptr<directx::ShadingTable>> shadingTables;
		std::shared_ptr<directx::RootSignatureManager> rootSignatureManager;
		std::shared_ptr<directx::RootSignatureManager> computeRSM;
		std::uint32_t currentShader;
	};
}