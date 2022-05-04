#pragma once

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

#include <vector>
#include <memory>
#include <cstdint>
#include <string>

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

	class PathTracingShading
		: public IDrawable
	{
	public:
		PathTracingShading(std::unique_ptr<sampler::ISampler> sampler, bool specularOnly);

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
		void accept(IVisitor* visitor) override;

		void setSpecularOnly(bool specularOnly);
		bool getSpecularOnly() const;

		void setPathFilter(std::uint32_t pathFilter);
		std::uint32_t getPathFilter() const;
		std::uint32_t getMinBounces() const;
		void setMinBounces(std::uint32_t minBounces);
		std::uint32_t getMaxBounces() const;
		void setMaxBounces(std::uint32_t maxBounces);

	private:
		void buildPipeline();
		void createShaderResources();
		void createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList, wrl::ComPtr<ID3D12Resource>& tempBuffer);

		// Common renderer resources
		RendererResources* rendererResources;

		// Path tracer descriptor stuff
		wrl::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		wrl::ComPtr<ID3D12RootSignature> globalEmptyRootSignature;
		wrl::ComPtr<ID3D12StateObject> stateObject;

		// Path tracing shader resources
		struct alignas(16) ConstBuff
		{
			DirectX::XMVECTOR u, v, w;
			DirectX::XMVECTOR position;
			DirectX::XMVECTOR direction;
			DirectX::XMVECTOR plane; // x, y and z (distance from point to plane)
			std::uint32_t seeds[2];
			mathematics::UVector2 winDimensions;
			std::uint32_t numLights;
			std::uint32_t frameNumber;
			std::uint32_t specularOnly;
			std::uint32_t pathFilter;
			std::uint32_t minBounces;
			std::uint32_t maxBounces;
		} constBuffer;

		std::vector<wrl::ComPtr<ID3D12Resource>> constantTempBuffer;
		wrl::ComPtr<ID3D12Resource> outputTexture;
		wrl::ComPtr<ID3D12Resource> radianceTexture;
		wrl::ComPtr<ID3D12Resource> constantBuffer;
		std::unique_ptr<sampler::ISampler> sampler;
		bool clear;

		// My helpers
		std::unique_ptr<directx::ShadingTable> shadingTable;
		std::shared_ptr<directx::RootSignatureManager> rootSignatureManager;
	};
}