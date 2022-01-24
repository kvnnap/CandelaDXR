#pragma once

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

#include "DirectX/CommandQueue.h"
#include "Scene/Scene.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class RasterShading
	{
	public:
		RasterShading(wrl::ComPtr<ID3D12Device9> pDevice, directx::CommandQueue &commandQueue, scene::Scene &scene, wrl::ComPtr<ID3D12Resource> sceneBuffer);

		void draw(wrl::ComPtr<ID3D12GraphicsCommandList6> pCurrentCommandList, wrl::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap, UINT currentBackBufferIndex);

	private:

		D3D12_VERTEX_BUFFER_VIEW bufferViews[3];
		D3D12_INDEX_BUFFER_VIEW indexView;
		D3D12_RECT scissorRect;
		D3D12_VIEWPORT viewport;

		wrl::ComPtr<ID3D12Device9> pDevice;
		directx::CommandQueue &commandQueue;
		scene::Scene& scene;
		wrl::ComPtr<ID3D12Resource> sceneBuffer;
		wrl::ComPtr<ID3D12RootSignature> rootSignature;
		wrl::ComPtr<ID3D12PipelineState> pipelineState;
	};
}