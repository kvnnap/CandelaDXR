#pragma once

#include <memory>
#include <vector>

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

#include "feanor/core/io/keyboard.h"
#include "feanor/core/io/mouse.h"
#include "Window/Window.h"
#include "DirectX/DxgiInfoManager.h"
#include "DirectX/CommandQueue.h"
#include "FpsCounter.h"
#include "Scene/Scene.h"
#include "IRenderer.h"
#include "IDrawable.h"
#include "Camera.h"

#include "ImGui/ImGuiSceneNode.h"
#include "ImGui/ImGuiMaterial.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class Renderer
		: public IRenderer
	{
	public:
		Renderer(scene::Scene *scene, Camera *camera, const mathematics::UVector2& windowDimensions, std::vector<IDrawable*> drawables, std::uint32_t adapterIndex, bool debugEnabled, bool breakEnabled, bool vsync);
		~Renderer();

		void init() override;
		void renderFrame() override;

	private:
		void initSceneResources();
		void updateCamera();
		std::vector<DirectX::XMFLOAT3X4> getMatrices(); 

		LRESULT wndCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		// Basic I/O and Window
		feanor::io::Keyboard keyboard;
		feanor::io::Mouse mouse;
		std::unique_ptr<ui::Window> window;
		mathematics::UVector2 windowDimensions;

		// DirectX
		const std::uint32_t adapterIndex;
		wrl::ComPtr<ID3D12Device> pDevice;
		std::unique_ptr<directx::CommandQueue> commandQueue;
		wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList;

		static constexpr UINT NumBackBuffers = 2;
		wrl::ComPtr<IDXGISwapChain> pSwapChain;
		wrl::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;
		wrl::ComPtr<ID3D12Resource> pRTVBackBuffers[NumBackBuffers];
		wrl::ComPtr<ID3D12Resource> pMatricesTempBackBuffers[NumBackBuffers];
		wrl::ComPtr<ID3D12Resource> pMaterialsTempBackBuffers[NumBackBuffers];

		// ImGui
		wrl::ComPtr<ID3D12DescriptorHeap> pImGuiDescriptorHeap;
		std::vector<imgui::ImGuiSceneNode> imguiSceneNodes;
		std::vector<imgui::ImGuiMaterial> imguiMaterials;

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
		wrl::ComPtr<ID3D12Resource> specularBuffer;
		wrl::ComPtr<ID3D12Resource> matrices;
		std::vector<wrl::ComPtr<ID3D12Resource>> textures;

		// TEST AREA
		std::vector<IDrawable*> drawables;

		// To pass
		RendererResources rendererResources;

		// Debug
		std::unique_ptr<directx::DxgiInfoManager> dxgiInfoManager;
		const bool debugEnabled;
		// Only enable this when a debugger is attached
		// otherwise on DX error, program calls abort/exit
		const bool breakEnabled;
		const bool vsync;
		bool viewImgui;
	};
}