#pragma once

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

#include <vector>

#include "DirectX/CommandQueue.h"
#include "Scene/Scene.h"

#include "Mathematics/Types.h"

#include "Camera.h"

#include "IDrawable.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class RasterShading
		: public IDrawable
	{
	public:
		RasterShading();

		void init(RendererResources* rendererResources) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList6> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;

	private:
		RendererResources* rendererResources;

		struct ConstBuff
		{
			DirectX::XMMATRIX MVP;
			DirectX::XMVECTOR CameraPosition;
		} constBuffer;

		D3D12_VERTEX_BUFFER_VIEW bufferViews[3];
		D3D12_INDEX_BUFFER_VIEW indexView;
		D3D12_RECT scissorRect;
		D3D12_VIEWPORT viewport;
		UINT dsvDescriptorSize;
		std::vector<wrl::ComPtr<ID3D12Resource>> pDepthBuffers;

		wrl::ComPtr<ID3D12DescriptorHeap> pDepthDescriptorHeap;
		wrl::ComPtr<ID3D12Resource> constantBuffer;
		std::vector<wrl::ComPtr<ID3D12Resource>> constantTempBuffer;
		wrl::ComPtr<ID3D12RootSignature> rootSignature;
		wrl::ComPtr<ID3D12PipelineState> pipelineState;

	};
}