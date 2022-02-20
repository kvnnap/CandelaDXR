#pragma once

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

#include <vector>
#include <memory>

#include "DirectX/DXUtil.h"

#include "DirectX/CommandQueue.h"
#include "DirectX/RootSignatureManager.h"
#include "DirectX/ShadingTable.h"
#include "Scene/Scene.h"

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
		LightTracingShading();

		void init(RendererResources* rendererResources) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList6> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList6> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;

	private:
		void buildPipeline();
		void createShaderResources();
		void createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList6> &commandList, wrl::ComPtr<ID3D12Resource> &tempResource);
		void buildTlas(wrl::ComPtr<ID3D12GraphicsCommandList6>& commandList, wrl::ComPtr<ID3D12Resource>& tempResource);

		RendererResources* rendererResources;

		struct alignas(16) ConstBuff
		{
			DirectX::XMVECTOR u, v, w;
			DirectX::XMVECTOR position;
			DirectX::XMVECTOR direction;
			DirectX::XMVECTOR plane; // x, y and z (distance from point to plane)
		} constBuffer;

		wrl::ComPtr<ID3D12DescriptorHeap> pDepthDescriptorHeap;
		wrl::ComPtr<ID3D12Resource> outputTexture;
		wrl::ComPtr<ID3D12Resource> constantBuffer;
		std::vector<wrl::ComPtr<ID3D12Resource>> constantTempBuffer;
		wrl::ComPtr<ID3D12RootSignature> rootSignature;
		wrl::ComPtr<ID3D12RootSignature> globalEmptyRootSignature;
		wrl::ComPtr<ID3D12StateObject> stateObject;;
		wrl::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		std::vector<directx::DXUtil::AccelerationStructureBuffers> blasBuffers;
		directx::DXUtil::AccelerationStructureBuffers tlasBuffers;
		std::vector<directx::DXUtil::TopLevelAccelerationData> tlasInstanceData;
		std::vector<wrl::ComPtr<ID3D12Resource>> tlasTempBuffer;

		// My helpers
		std::shared_ptr<directx::RootSignatureManager> rootSignatureManager;
		std::unique_ptr<directx::ShadingTable> shadingTable;
	};
}