#pragma once

#include <vector>
#include <memory>

#include "DirectX/Types.h"
#include "DirectX/Resource.h"
#include "DirectX/ResourceManager.h"
#include <dxgi1_6.h>
#include "Scene/Scene.h"
#include "Mathematics/Types.h"
#include "Camera.h"
#include "Window/Window.h"
#include "feanor/core/io/ikeyreader.h"
#include "feanor/core/io/imousereader.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class AccelerationStructure;
	class IDrawable;
	class Renderer;

	struct RendererResources
	{
		Renderer* renderer{};
		wrl::ComPtr<ID3D12Device> pDevice;
		wrl::ComPtr<ID3D12Resource> sceneBuffer;
		wrl::ComPtr<ID3D12Resource> materialBuffer;
		wrl::ComPtr<ID3D12Resource> faceAttributeBuffer;
		wrl::ComPtr<ID3D12Resource> lightBuffer;
		wrl::ComPtr<ID3D12Resource> specularBuffer;
		wrl::ComPtr<ID3D12Resource> matrices;
		wrl::ComPtr<ID3D12Resource> normalMatrices;
		wrl::ComPtr<ID3D12Resource> externalLights;
		wrl::ComPtr<IDXGIAdapter> adapter;
		std::vector<directx::Resource> textures;
		wrl::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;
		directx::Resource* pRTVRad{};
		directx::Resource* pRTVDiff{};
		directx::Resource* pRTVSpec{};
		directx::Resource* pRTVCaus{};
		directx::CommandQueue* commandQueue{};
		mathematics::UVector2 winDimensions{};
		UINT numBackBuffers{};
		scene::Scene* scene{};
		Camera* camera{};
		AccelerationStructure* accelerationStructure{};
		UINT currentBackBufferIndex{};
		std::unique_ptr<directx::ResourceManager> resourceManager;
		ui::Window* window{};
		std::vector<IDrawable*>* drawables{};
		std::uint64_t frameNumber{};
		std::vector<candela::scene::Light> processedExternalLights{};
		feanor::io::IKeyReader* keyboard{};
		feanor::io::IMouseReader* mouse{};

		directx::DXResource& getTempResource()
		{
			return resourceManager->getTempResource(currentBackBufferIndex);
		}
	};
}