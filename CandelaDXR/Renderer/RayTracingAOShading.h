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

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class RayTracingAOShading
		: public Drawable
	{
	public:
		RayTracingAOShading(std::vector<std::string> inputs, std::vector<std::string> outputs);

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
		void accept(IVisitor* visitor) override;

		void setCameraPosition(const DirectX::XMVECTOR& cameraPos);

	private:
		void buildPipeline();
		void createDescriptorTable();
		void createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList);

		// Common renderer resources
		RendererResources* rendererResources;
		directx::Resource* cdfMask;

		// Path tracer descriptor stuff
		wrl::ComPtr<ID3D12RootSignature> globalRootSignature;
		wrl::ComPtr<ID3D12StateObject> stateObject;

		// Path tracing shader resources
		struct alignas(16) ConstBuff
		{
			DirectX::XMVECTOR cameraPos;
		} constBuffer;

		std::vector<std::string> inputs; 
		std::vector<std::string> outputs;
		std::vector<directx::Resource*> inputResources;

		// My helpers
		std::shared_ptr<directx::DescriptorHeap> descHeapManager;
		std::unique_ptr<directx::ShadingTable> shadingTable;
		std::shared_ptr<directx::RootSignatureManager> rootSignatureManager;
	};
}