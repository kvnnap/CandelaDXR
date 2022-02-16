#pragma once

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

#include <vector>

#include "DirectX/DXUtil.h"

#include "DirectX/CommandQueue.h"
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

	private:
		RendererResources* rendererResources;

		struct ConstBuff
		{
			DirectX::XMMATRIX MVP;
			DirectX::XMVECTOR CameraPosition;
		} constBuffer;


		wrl::ComPtr<ID3D12DescriptorHeap> pDepthDescriptorHeap;
		wrl::ComPtr<ID3D12Resource> constantBuffer;
		std::vector<wrl::ComPtr<ID3D12Resource>> constantTempBuffer;
		wrl::ComPtr<ID3D12RootSignature> rootSignature;
		wrl::ComPtr<ID3D12RootSignature> globalEmptyRootSignature;
		wrl::ComPtr<ID3D12PipelineState> pipelineState;
		wrl::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		std::vector<directx::DXUtil::AccelerationStructureBuffers> blasBuffers;
		directx::DXUtil::AccelerationStructureBuffers tlasBuffers;
	};
}