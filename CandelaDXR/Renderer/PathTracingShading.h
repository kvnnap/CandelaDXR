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

#include "feanor/anvil/core/anvil.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class PathTracingShading
		: public Drawable
	{
	public:
		PathTracingShading(std::unique_ptr<sampler::ISampler> sampler, bool specularOnly);

		void init(RendererResources* rendererResources, directx::DXCommandList& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(directx::DXCommandList pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(directx::DXCommandList pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
		void accept(IVisitor* visitor) override;
		std::uint32_t getBufferUsage() const override;

		void setSpecularOnly(bool specularOnly);
		bool getSpecularOnly() const;

		void setPathFilter(std::uint32_t pathFilter);
		std::uint32_t getPathFilter() const;
		std::uint32_t getMinBounces() const;
		void setMinBounces(std::uint32_t minBounces);
		std::uint32_t getMaxBounces() const;
		void setMaxBounces(std::uint32_t maxBounces);

		// Anvil
		ANVIL_CODE_RAW(
			struct alignas(16) PathTracingIntersectionContext {
				// Ray - XMVECTOR's are pods
				DirectX::XMVECTOR origin;
				DirectX::XMVECTOR direction;
				float tMin;
				float tMax;
				float tHit;
				float rayProbability;

				DirectX::XMVECTOR radiance;
				DirectX::XMVECTOR unitNormal;

				uint32_t rayDepth;
				uint32_t rayType;
				uint32_t primitiveId;
				uint32_t materialId;
			};

			struct alignas(16) PathTracingPath {
				uint32_t debugId;
				uint32_t numRays;
				uint32_t pixelX;
				uint32_t pixelY;
				uint32_t seed1;
				uint32_t seed2;
				uint32_t padding[2];
				DirectX::XMVECTOR totalRadiance;
				PathTracingIntersectionContext pathTracingIntersectionContext[16];
			};
		)
	private:
		void buildPipeline();
		void createShaderResources();
		void createShaderTable(directx::DXCommandList& commandList, directx::DXResource& tempBuffer);

		// Common renderer resources
		RendererResources* rendererResources;

		// Path tracer descriptor stuff
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
			std::uint32_t numExternalLights;
			std::uint32_t numTotalLights;
			std::uint32_t frameNumber;
			std::uint32_t specularOnly;
			std::uint32_t pathFilter;
			std::uint32_t minBounces;
			std::uint32_t maxBounces;
			ANVIL_CODE_RAW(
				std::uint32_t debugPixelCoords[2];
				std::uint32_t debugPixel;
				std::uint32_t padding;
			)
		} constBuffer;

		directx::Resource* diffTexture;
		directx::Resource* specTexture;
		directx::Resource* causTexture;
		directx::Resource* rayHitT;
		directx::Resource* prngState;
		directx::DXResource constantBuffer;
		std::unique_ptr<sampler::ISampler> sampler;
		bool clear;

		// My helpers
		std::shared_ptr<directx::DescriptorHeap> descHeapManager;
		std::unique_ptr<directx::ShadingTable> shadingTable;
		std::shared_ptr<directx::RootSignatureManager> rootSignatureManager;

		// Anvil
		ANVIL_CODE_RAW(
			std::shared_ptr<feanor::anvil::Entity> pathEntity;
			PathTracingPath localDebugPathTracingPath;
			directx::Resource* outputAnvilBuffer;
			directx::Resource* readbackAnvilBuffer;
		)
	};
}