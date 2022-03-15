#pragma once

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

#include <vector>
#include <memory>
#include <cstdint>

#include "DirectX/DXUtil.h"

#include "DirectX/CommandQueue.h"
#include "DirectX/RootSignatureManager.h"
#include "DirectX/ShadingTable.h"
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

		void init(RendererResources* rendererResources) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;

	private:
		void buildPipeline();
		void createShaderResources();
		void createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList> &commandList, wrl::ComPtr<ID3D12Resource> &tempResource);
		void buildTlas(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList, wrl::ComPtr<ID3D12Resource>& tempResource);
		void generateIrrToRadTexture(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList, wrl::ComPtr<ID3D12Resource>& tempResource);

		mathematics::Vector2 toSensorSpace(std::uint32_t x, std::uint32_t y) const;
		float cosIntegral(std::uint32_t x, std::uint32_t y) const;

		// Common renderer resources
		RendererResources* rendererResources;
		
		// Light tracer descriptor stuff
		wrl::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		wrl::ComPtr<ID3D12RootSignature> globalEmptyRootSignature;
		wrl::ComPtr<ID3D12StateObject> stateObject;

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
		} constBuffer;
		mathematics::UVector2 lightSamples;
		std::vector<wrl::ComPtr<ID3D12Resource>> constantTempBuffer;
		wrl::ComPtr<ID3D12Resource> outputTexture;
		wrl::ComPtr<ID3D12Resource> constantBuffer;
		wrl::ComPtr<ID3D12Resource> irrToRad;
		wrl::ComPtr<ID3D12Resource> irradianceTexture;
		wrl::ComPtr<ID3D12Resource> irradianceDataStructure;
		std::unique_ptr<sampler::ISampler> sampler;
		bool clear;

		// Acceleration structure
		std::vector<directx::DXUtil::AccelerationStructureBuffers> blasBuffers;
		directx::DXUtil::AccelerationStructureBuffers tlasBuffers;
		std::vector<directx::DXUtil::TopLevelAccelerationData> tlasInstanceData;
		std::vector<wrl::ComPtr<ID3D12Resource>> tlasTempBuffer;

		// Compute shader
		wrl::ComPtr<ID3D12DescriptorHeap> computeDescriptorHeap;
		wrl::ComPtr<ID3D12RootSignature> computeRootSignature;
		wrl::ComPtr<ID3D12PipelineState> computePipelineState;

		// My helpers
		std::unique_ptr<directx::ShadingTable> shadingTable;
		std::shared_ptr<directx::RootSignatureManager> computeRSM;
	};
}