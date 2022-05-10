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
#include "DirectX/RootSignatureManager.h"
#include "DirectX/Resource.h"
#include "FpsCounter.h"
#include "Scene/Scene.h"
#include "IRenderer.h"
#include "IDrawable.h"
#include "Camera.h"
#include "Chain/IChain.h"

#include "ImGui/ImGuiSceneNode.h"
#include "ImGui/ImGuiMaterial.h"
#include "ImGui/ImGuiShading.h"

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
		template<class T>
		using ComPtrVec = std::vector<wrl::ComPtr<T>>;
		
		using ResPtrVec = std::vector<std::shared_ptr<directx::Resource>>;

		void initSceneResources();
		void initShaders();
		void createShaderResources();
		void updateCamera();
		void resize();
		void resizeFloatTargetTextures();
		void refreshMaterialResources();
		directx::DXResource& getTempResource();
		std::vector<DirectX::XMFLOAT3X4> getMatrices();
		std::vector<DirectX::XMFLOAT3X3> getNormalMatrices();
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
		wrl::ComPtr<IDXGIFactory> dxgiFactory;
		wrl::ComPtr<IDXGISwapChain> pSwapChain;
		wrl::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;
		std::unique_ptr<directx::Resource> pRadAccumulator;
		std::unique_ptr<directx::Resource> pRTV8BitBackBuffer;
		ResPtrVec pRTVBackBuffers;
		ResPtrVec pRTVRadBackBuffers;

		// ImGui
		wrl::ComPtr<ID3D12DescriptorHeap> pImGuiDescriptorHeap;
		std::vector<imgui::ImGuiSceneNode> imguiSceneNodes;
		std::vector<imgui::ImGuiMaterial> imguiMaterials;
		std::vector<imgui::ImGuiShading> imguiShaders;

		// Constants and integral values
		UINT rtvDescriptorSize;
		UINT currentBackBufferIndex;
		std::vector<uint64_t> frameFenceValues;

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
		wrl::ComPtr<ID3D12Resource> normalMatrices;
		std::vector<directx::Resource> textures;

		// Chain
		std::vector<std::unique_ptr<chain::IChain>> chain;

		// Shaders
		std::shared_ptr<directx::RootSignatureManager> computeRSM;
		wrl::ComPtr<ID3D12RootSignature> computeRootSignature;
		wrl::ComPtr<ID3D12PipelineState> computePipelineState;
		wrl::ComPtr<ID3D12DescriptorHeap> computeDescriptorHeap;

		// TEST AREA
		std::vector<IDrawable*> drawables;
		std::vector<std::unique_ptr<IResource>> resources;

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