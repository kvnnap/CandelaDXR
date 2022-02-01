#pragma once

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

#include <vector>

#include "DirectX/CommandQueue.h"
#include "Scene/Scene.h"

#include "Camera.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class RasterShading
	{
	public:
		RasterShading(
			wrl::ComPtr<ID3D12Device9> pDevice,
			directx::CommandQueue &commandQueue,
			scene::Scene &scene,
			wrl::ComPtr<ID3D12Resource> sceneBuffer,
			wrl::ComPtr<ID3D12Resource> materialBuffer,
			wrl::ComPtr<ID3D12Resource> faceAttributeBuffer,
			wrl::ComPtr<ID3D12Resource> lightBuffer,
			UINT numBackBuffers,
			Camera& camera);

		void draw(wrl::ComPtr<ID3D12GraphicsCommandList6> pCurrentCommandList, wrl::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap, UINT currentBackBufferIndex);

	private:

		struct ConstBuff
		{
			DirectX::XMMATRIX MVP;
			DirectX::XMVECTOR CameraPosition;
		} constBuffer;

		D3D12_VERTEX_BUFFER_VIEW bufferViews[3];
		D3D12_INDEX_BUFFER_VIEW indexView;
		D3D12_RECT scissorRect;
		D3D12_VIEWPORT viewport;
		const UINT numBackBuffers;
		UINT dsvDescriptorSize;
		std::vector<wrl::ComPtr<ID3D12Resource>> pDepthBuffers;

		wrl::ComPtr<ID3D12Device9> pDevice;
		wrl::ComPtr<ID3D12DescriptorHeap> pDepthDescriptorHeap;
		directx::CommandQueue &commandQueue;
		scene::Scene& scene;
		wrl::ComPtr<ID3D12Resource> sceneBuffer;
		wrl::ComPtr<ID3D12Resource> materialBuffer;
		wrl::ComPtr<ID3D12Resource> faceAttributeBuffer;
		wrl::ComPtr<ID3D12Resource> lightBuffer;
		wrl::ComPtr<ID3D12Resource> constantBuffer;
		std::vector<wrl::ComPtr<ID3D12Resource>> constantTempBuffer;
		wrl::ComPtr<ID3D12RootSignature> rootSignature;
		wrl::ComPtr<ID3D12PipelineState> pipelineState;
		Camera& camera;

	};
}