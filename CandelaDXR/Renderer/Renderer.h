#pragma once

#include <memory>
#include <vector>

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

#include "feanor/core/io/keyboard.h"
#include "feanor/core/io/mouse.h"
#include "Window/Window.h"
#include "DirectX/CommandQueue.h"
#include "FpsCounter.h"
#include "Scene/Scene.h"
#include "IRenderer.h"
#include "IDrawable.h"
#include "Camera.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class Renderer
		: public IRenderer
	{
	public:
		Renderer(scene::Scene *scene, Camera *camera, const mathematics::UVector2& windowDimensions, std::vector<IDrawable*> drawables);
		~Renderer();

		void renderFrame() override;

	private:
		void initSceneResources();
		void updateCamera();

		// Basic I/O and Window
		feanor::io::Keyboard keyboard;
		feanor::io::Mouse mouse;
		std::unique_ptr<ui::Window> window;

		// DirectX
		wrl::ComPtr<ID3D12Device9> pDevice;
		std::unique_ptr<directx::CommandQueue> commandQueue;
		wrl::ComPtr<ID3D12GraphicsCommandList6> pCurrentCommandList;

		static constexpr UINT NumBackBuffers = 2;
		wrl::ComPtr<IDXGISwapChain4> pSwapChain;
		wrl::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;
		wrl::ComPtr<ID3D12Resource> pRTVBackBuffers[NumBackBuffers];

		// Constants and integral values
		UINT rtvDescriptorSize;
		UINT currentBackBufferIndex;
		uint64_t frameFenceValues[NumBackBuffers];

		// Stats
		FpsCounter fpsCounter;

		// Scene
		scene::Scene *scene;
		Camera *camera;
		wrl::ComPtr<ID3D12Resource> sceneBuffer;
		wrl::ComPtr<ID3D12Resource> faceAttributeBuffer;
		wrl::ComPtr<ID3D12Resource> materialBuffer;
		wrl::ComPtr<ID3D12Resource> lightBuffer;

		// TEST AREA
		std::vector<IDrawable*> drawables;

		// To pass
		RendererResources rendererResources;
	};
}