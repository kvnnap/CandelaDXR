#pragma once

#include <vector>
#include <memory>

#include "DirectX/Types.h"
#include "DirectX/Resource.h"
#include "DirectX/ResourceManager.h"
#include "Scene/Scene.h"
#include "Mathematics/Types.h"
#include "Camera.h"
#include "Window/Window.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class AccelerationStructure;
	class IDrawable;

	struct RendererResources
	{
		wrl::ComPtr<ID3D12Device> pDevice;
		wrl::ComPtr<ID3D12Resource> sceneBuffer;
		wrl::ComPtr<ID3D12Resource> materialBuffer;
		wrl::ComPtr<ID3D12Resource> faceAttributeBuffer;
		wrl::ComPtr<ID3D12Resource> lightBuffer;
		wrl::ComPtr<ID3D12Resource> specularBuffer;
		wrl::ComPtr<ID3D12Resource> matrices;
		wrl::ComPtr<ID3D12Resource> normalMatrices;
		std::vector<directx::Resource> textures;
		wrl::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;
		std::shared_ptr<directx::Resource> pRTVRadBackBuffer;
		directx::CommandQueue* commandQueue;
		mathematics::UVector2 winDimensions;
		UINT numBackBuffers;
		scene::Scene* scene;
		Camera* camera;
		AccelerationStructure* accelerationStructure;
		UINT currentBackBufferIndex;
		std::unique_ptr<directx::ResourceManager> resourceManager;
		ui::Window* window;
		std::vector<IDrawable*> *drawables;

		directx::DXResource& getTempResource()
		{
			return resourceManager->getTempResource(currentBackBufferIndex);
		}
	};
}