#pragma once

#include <cstdint>


#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

#include <vector>

#include "DirectX/CommandQueue.h"
#include "Scene/Scene.h"

#include "Mathematics/Types.h"

#include "Camera.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	struct RendererResources
	{
		wrl::ComPtr<ID3D12Device9> pDevice;
		wrl::ComPtr<ID3D12Resource> sceneBuffer;
		wrl::ComPtr<ID3D12Resource> materialBuffer;
		wrl::ComPtr<ID3D12Resource> faceAttributeBuffer;
		wrl::ComPtr<ID3D12Resource> lightBuffer;
		wrl::ComPtr<ID3D12Resource> matrices;
		std::vector<wrl::ComPtr<ID3D12Resource>> textures;
		wrl::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;
		std::vector<wrl::ComPtr<ID3D12Resource>> pRTVBackBuffers;
		directx::CommandQueue *commandQueue;
		mathematics::UVector2 winDimensions;
		UINT numBackBuffers;
		scene::Scene *scene;
		Camera *camera;
	};

	class IDrawable
	{
	public:
		virtual ~IDrawable() = default;
		virtual void init(RendererResources *rendererResources) = 0;
		virtual void draw(wrl::ComPtr<ID3D12GraphicsCommandList6> pCurrentCommandList, std::uint32_t currentBackBufferIndex) = 0;
		virtual void onChange(wrl::ComPtr<ID3D12GraphicsCommandList6> pCurrentCommandList, std::uint32_t currentBackBufferIndex) = 0;
	};
}