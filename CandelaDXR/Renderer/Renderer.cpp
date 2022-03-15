#include "Renderer.h"

#include "Exception/WindowException.h"
#include "Mathematics/Types.h"

#include "DirectX/DxUtil.h"
#include "DirectX/d3dx12.h"
#include <DirectXMath.h>
#include <dxgidebug.h>

#include "System/DllManager.h"

#include "imgui/imgui.h"
#include "ImGui/Backend/imgui_impl_win32.h"
#include "ImGui/Backend/imgui_impl_dx12.h"

#include <iostream>

using std::make_unique;
using std::to_string;
using std::vector;

using Microsoft::WRL::ComPtr;

using feanor::io::Keyboard;
using feanor::io::Mouse;

using candela::directx::DXUtil;
using candela::directx::CommandQueue;

using candela::mathematics::Vector2;
using candela::mathematics::UVector2;
using candela::mathematics::Vector3;

using candela::ui::Window;
using candela::scene::Scene;
using candela::scene::Material;
using candela::scene::FaceAttributes;
using candela::scene::AreaLight;
using candela::scene::SpecularPrimitive;
using candela::renderer::Renderer;
using candela::renderer::Camera;
using candela::renderer::IDrawable;
using candela::renderer::ChangeEvent;
using candela::renderer::ChangeEvent_t;
using candela::renderer::imgui::ImGuiSceneNode;

using DirectX::XMVectorSet;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Renderer::Renderer(Scene *scene, Camera *camera, const UVector2 &windowDimensions, vector<IDrawable*> p_drawables, uint32_t adapterIndex, bool debugEnabled, bool breakEnabled, bool vsync)
	: windowDimensions(windowDimensions),
	  adapterIndex(adapterIndex),
	  rtvDescriptorSize(),
	  currentBackBufferIndex(),
	  frameFenceValues(),
	  scene(scene),
	  camera(camera),
	  drawables(std::move(p_drawables)),
	  debugEnabled(debugEnabled),
	  breakEnabled(breakEnabled),
	  vsync(vsync),
	  viewImgui()
{
}

Renderer::~Renderer()
{
	if (commandQueue)
		commandQueue->flush();

	// Cannot destroy swapchain in full screen mode
	pSwapChain->SetFullscreenState(false, nullptr);
	// Uncomment to analyse resources
	//if (debugEnabled)
	//{
	//	HRESULT hr;
	//	system::DllManager dxgiDebugDll("DXGIDebug.dll");
	//	auto fn = dxgiDebugDll.getFunction<decltype(DXGIGetDebugInterface)>("DXGIGetDebugInterface");
	//	ComPtr<IDXGIDebug> debugInterface;
	//	GFXTHROWIFFAILED(fn(IID_PPV_ARGS(&debugInterface)));
	//	GFXTHROWIFFAILED(debugInterface->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL));
	//}
	
	if (dxgiInfoManager && dxgiInfoManager->hasMessages())
	{
		std::cout << "Printing messages from IDXGIInfoQueue:" << std::endl;
		for (const auto& msg : dxgiInfoManager->getMessages())
			std::cout << msg << std::endl;
	}
}

void Renderer::init()
{
	camera->setAspectRatio(static_cast<float>(windowDimensions.x) / windowDimensions.y);
	window = make_unique<Window>("CandelaDXR", windowDimensions.x, windowDimensions.y, &keyboard, &mouse);
	using namespace std::placeholders;
	window->addWndProcCallback(std::bind(&Renderer::wndCallback, this, _1, _2, _3, _4));

	// Allocate 
	pRTVBackBuffers.resize(NumBackBuffers);
	pMatricesTempBackBuffers.resize(NumBackBuffers);
	pMaterialsTempBackBuffers.resize(NumBackBuffers);
	pLightsTempBackBuffers.resize(NumBackBuffers);
	pFaceAttrTempBackBuffers.resize(NumBackBuffers);
	pSpecularsTempBackBuffers.resize(NumBackBuffers);

	frameFenceValues.resize(NumBackBuffers);

	// Init DirectX Debugging
	if (debugEnabled)
	{
		dxgiInfoManager = make_unique<directx::DxgiInfoManager>();
		DXUtil::enableDebugLayer(); // Not sure what happens if called twice
	}

	dxgiFactory = DXUtil::createDXGIFactory(debugEnabled);

	// Get DX12 compatible hardware device - Adapter contains info about the actual device
	D3D_FEATURE_LEVEL featureLevel;
	auto adapter = DXUtil::getAdapterLatestFeatureLevel(dxgiFactory, &featureLevel, false, adapterIndex);
	pDevice = DXUtil::createDeviceFromAdapter(adapter, featureLevel);

	// Enable debug messages in debug mode for this device
	if (debugEnabled)
		DXUtil::setupDebugLayer(pDevice, breakEnabled);

	// Create command queue, with command list and command allocators
	commandQueue = make_unique<CommandQueue>(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);

	// Create swap chain
	pSwapChain = DXUtil::createSwapChain(dxgiFactory, commandQueue->getCommandQueue(), window->getHandle(), NumBackBuffers);

	// Create descriptor heap for render target view
	pRTVDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, NumBackBuffers, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	pRTVDescriptorHeap->SetName(L"RTV Descriptor Heap");

	// Create render target Views
	rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	pRTVBackBuffers = DXUtil::createRenderTargetViews(pDevice, pRTVDescriptorHeap, pSwapChain, NumBackBuffers);

	// Upload scene resources
	initSceneResources();

	// ImGui
	pImGuiDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, 1u, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	pImGuiDescriptorHeap->SetName(L"ImGui Descriptor Heap");
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(window->getHandle());
	ImGui_ImplDX12_Init(pDevice.Get(), NumBackBuffers, DXGI_FORMAT_R8G8B8A8_UNORM, pImGuiDescriptorHeap.Get(),
		pImGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		pImGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	ImGui::StyleColorsDark();
	window->addWndProcCallback(ImGui_ImplWin32_WndProcHandler);
	for (auto& child : scene->getSceneGraph().Children)
		imguiSceneNodes.emplace_back(child, *scene);
	for (size_t i = 0; i < scene->getMaterials().size(); ++i)
	{
		auto& mat = scene->getMaterials()[i];
		imguiMaterials.emplace_back(mat, i);
	}

	// Prepare struct to share with drawables
	rendererResources = RendererResources
	{
		.pDevice = pDevice,
		.sceneBuffer = sceneBuffer,
		.materialBuffer = materialBuffer,
		.faceAttributeBuffer = faceAttributeBuffer,
		.lightBuffer = lightBuffer,
		.specularBuffer = specularBuffer,
		.matrices = matrices,
		.textures = textures,
		.pRTVDescriptorHeap = pRTVDescriptorHeap,
		.pRTVBackBuffers = pRTVBackBuffers,
		.commandQueue = commandQueue.get(),
		.winDimensions = windowDimensions,
		.numBackBuffers = NumBackBuffers,
		.scene = scene,
		.camera = camera
	};

	// Init drawables
	for (IDrawable* drawable : drawables)
		drawable->init(&rendererResources);
}

void Renderer::renderFrame()
{
	// Clear frame and start frame
	auto &rtvBackBuffer = pRTVBackBuffers[currentBackBufferIndex];

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(rtvBackBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	pCurrentCommandList = commandQueue->getCommandList();
	pCurrentCommandList->ResourceBarrier(1, &barrier);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), currentBackBufferIndex, rtvDescriptorSize);
	FLOAT color[] = { 0.f, 0.f, 0.f, 1.0f };
	pCurrentCommandList->ClearRenderTargetView(rtvDescriptorHandle, color, 0, nullptr);

	// ImGui
	if (keyboard.hasKeyChanged('Q') && keyboard.isKeyPressed('Q'))
		viewImgui = !viewImgui;
	ChangeEvent_t changeEvent{};
	if (viewImgui)
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Transforms");
		for (auto& imguiSceneNode : imguiSceneNodes)
		{
			imguiSceneNode.drawUi();
			if (imguiSceneNode.hasChanged())
				changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::Transformation);
		}
		ImGui::End();

		ImGui::Begin("Materials");
		for (auto& imguiMaterial : imguiMaterials)
		{
			imguiMaterial.drawUi();
			if(imguiMaterial.hasChanged())
				changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::SceneUpdate);
			if(imguiMaterial.hasMajorChange())
				changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::SceneChange);
		}
		ImGui::End();

		ImGui::Render();
	}

	constexpr auto flags = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	
	// Update Transforms
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::Transformation))
	{
		auto gMatrices = getMatrices();
		DXUtil::updateDataInDefaultHeap(
			pDevice,
			pCurrentCommandList,
			matrices,
			pMatricesTempBackBuffers[currentBackBufferIndex],
			getMatrices().data(),
			sizeof(decltype(gMatrices)::value_type) * gMatrices.size(),
			flags,
			flags);
	}

	// Update material, face attributes and lights
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::SceneUpdate))
	{
		DXUtil::updateDataInDefaultHeap(pDevice, pCurrentCommandList, materialBuffer,
			pMaterialsTempBackBuffers[currentBackBufferIndex], scene->getMaterials().data(), 
			sizeof(Material) * scene->getMaterials().size(), flags, flags);
		
	}

	// On Scene change need to update lights, specs and face attributes
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::SceneChange))
	{
		scene->recalculateLightsAndFaceAttributes();
		commandQueue->flush(); // This ensures no resources are in the GPUs queue
		refreshMaterialResources();
	}

	// Draw
	updateCamera();
	for (IDrawable* drawable : drawables)
	{
		if (changeEvent)
			drawable->onChange(pCurrentCommandList, currentBackBufferIndex, changeEvent);
		drawable->draw(pCurrentCommandList, currentBackBufferIndex);
	}

	// ImGui Render
	if (viewImgui)
	{
		pCurrentCommandList->OMSetRenderTargets(1u, &rtvDescriptorHandle, FALSE, nullptr);
		pCurrentCommandList->SetDescriptorHeaps(1u, pImGuiDescriptorHeap.GetAddressOf());
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCurrentCommandList.Get());
	}

	// End frame
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(rtvBackBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	pCurrentCommandList->ResourceBarrier(1, &barrier);
	frameFenceValues[currentBackBufferIndex] = commandQueue->executeCommandList(pCurrentCommandList);
	pCurrentCommandList.Reset();

	HRESULT hr;
	// Present may wait on or execute the message pump when mode changes (fullscreen to windowed, etc)
	// Therefore, make sure RTV buffers are not referenced here
	GFXTHROWIFFAILED(pSwapChain->Present(vsync ? 1u : 0u, 0u));
	ComPtr<IDXGISwapChain3> pSwapChain3;
	GFXTHROWIFFAILED(pSwapChain.As(&pSwapChain3));
	currentBackBufferIndex = pSwapChain3->GetCurrentBackBufferIndex();
	commandQueue->waitForFenceValue(frameFenceValues[currentBackBufferIndex]);

	// Stats
	if ((changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::Transformation)) || camera->hasChanged())
		fpsCounter.resetFrameCount();
	if (fpsCounter.hitFrame())
		window->setWindowName("CandelaDXR - Frames: " + to_string(fpsCounter.getFrameCount()) + " FPS: " + to_string(fpsCounter.getFramesPerSecond()));

	// Reset camera
	camera->resetChanged();
}

void Renderer::initSceneResources()
{
	auto totalSize = scene->getVerticesSizeBytes() + scene->getTextureCoordsSizeBytes()
				   + scene->getNormalsSizeBytes() + scene->getIndicesSizeBytes();
	if (totalSize == 0 || scene->getFaceAttributes().empty())
		ThrowException("Scene is empty - nothing to render");

	wrl::ComPtr<ID3D12Resource> tempVB;
	sceneBuffer = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, totalSize, D3D12_RESOURCE_STATE_COPY_DEST);
	sceneBuffer->SetName(L"Scene Buffer");
	auto tempResource = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_UPLOAD, totalSize, D3D12_RESOURCE_STATE_GENERIC_READ);
	std::uint8_t* data;
	auto readRange = D3D12_RANGE(0, 0);
	tempResource->Map(0, &readRange, reinterpret_cast<void**>(&data));
	memcpy(data + scene->getVerticesOffset(), scene->getVertices().data(), scene->getVerticesSizeBytes());
	memcpy(data + scene->getTextureCoordsOffset(), scene->getTextureCoords().data(), scene->getTextureCoordsSizeBytes());
	memcpy(data + scene->getNormalsOffset(), scene->getNormals().data(), scene->getNormalsSizeBytes());
	memcpy(data + scene->getIndicesOffset(), scene->getIndices().data(), scene->getIndicesSizeBytes());
	tempResource->Unmap(0, nullptr);
	pCurrentCommandList = commandQueue->getCommandList();
	pCurrentCommandList->CopyResource(sceneBuffer.Get(), tempResource.Get());

	// Upload materials and face attributes (in separate buffers otherwise we have to take care of alignment)
	constexpr auto flags = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	
	const auto& mats = scene->getMaterials().empty() ? vector<Material>(1ULL) : scene->getMaterials();
	materialBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList,
		pMaterialsTempBackBuffers[currentBackBufferIndex], mats.data(), sizeof(Material) * mats.size(), flags);
	materialBuffer->SetName(L"Material Buffer");

	refreshMaterialResources();

	// Copy Matrices
	const auto& matrs = scene->getSceneGraph().Children.empty() ? vector<DirectX::XMFLOAT3X4>(1ULL) : getMatrices();
	wrl::ComPtr<ID3D12Resource> tempMatrices;
	matrices = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, tempMatrices,
		matrs.data(), sizeof(DirectX::XMFLOAT3X4) * matrs.size(), flags);
	matrices->SetName(L"Matrices Buffer");

	// Upload textures
	std::vector<wrl::ComPtr<ID3D12Resource>> texTempBuffer (scene->getTextures().size());
	auto tempTexBuffer = texTempBuffer.begin();
	for (const auto& texture : scene->getTextures())
	{
		textures.push_back(DXUtil::uploadTextureDataToDefaultHeap(
			pDevice,
			pCurrentCommandList,
			*tempTexBuffer++,
			texture.data(),
			texture.getWidth(),
			texture.getHeight(),
			texture.getChannels(),
			DXGI_FORMAT_R8G8B8A8_UNORM, flags));
		textures.back()->SetName(L"Texture");
	}

	if (textures.empty())
	{
		textures.push_back(DXUtil::createTextureCommittedResource(
			pDevice, D3D12_HEAP_TYPE_DEFAULT, 1, 1, flags, D3D12_RESOURCE_FLAG_NONE, DXGI_FORMAT_R8G8B8A8_UNORM));
		textures.back()->SetName(L"Empty Texture");
	}


	auto fenceValue = commandQueue->executeCommandList(pCurrentCommandList);
	commandQueue->waitForFenceValue(fenceValue);
}

void Renderer::updateCamera()
{
	auto getValueIfPressed = [this](char key, float value) {
		return keyboard.isKeyPressed(key) ? value : 0.f;
	};

	constexpr float unitsPerSec = 3.f;

	float deltaUnits = fpsCounter.getLastFrameTime() / 1000.f * unitsPerSec;
	if (keyboard.isKeyPressed('D') || keyboard.isKeyPressed('W'))
		camera->incrementPositionAlongDirection(getValueIfPressed('D', -deltaUnits), getValueIfPressed('W', deltaUnits));
	if (keyboard.isKeyPressed('A') || keyboard.isKeyPressed('S'))
		camera->incrementPositionAlongDirection(getValueIfPressed('A', deltaUnits), getValueIfPressed('S', -deltaUnits));

	deltaUnits = fpsCounter.getLastFrameTime() / 1000.f;
	if (keyboard.isKeyPressed('L') || keyboard.isKeyPressed('I'))
		camera->incrementDirection(getValueIfPressed('L', -deltaUnits), getValueIfPressed('I', -deltaUnits));
	if (keyboard.isKeyPressed('J') || keyboard.isKeyPressed('K'))
		camera->incrementDirection(getValueIfPressed('J', deltaUnits), getValueIfPressed('K', deltaUnits));
}

void Renderer::resize()
{
	// Wait for all GPU operations to complete
	commandQueue->flush();
	rendererResources.pRTVBackBuffers.clear();
	pRTVBackBuffers.clear();
	currentBackBufferIndex = 0;
	camera->setAspectRatio(static_cast<float>(windowDimensions.x) / windowDimensions.y);
	HRESULT hr;
	auto flags = DXUtil::checkTearingSupport(dxgiFactory) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	GFXTHROWIFFAILED(pSwapChain->ResizeBuffers(NumBackBuffers, windowDimensions.x, windowDimensions.y, DXGI_FORMAT_UNKNOWN, flags));
	rendererResources.pRTVBackBuffers = pRTVBackBuffers = DXUtil::createRenderTargetViews(pDevice, pRTVDescriptorHeap, pSwapChain, NumBackBuffers);
	// Resize drawables
	for (IDrawable* drawable : drawables)
		drawable->onResize();
}

vector<DirectX::XMFLOAT3X4> Renderer::getMatrices()
{
	vector<DirectX::XMFLOAT3X4> localMatrices(scene->getSceneGraph().Children.size());
	auto ptMat = localMatrices.begin();
	for (auto child : scene->getSceneGraph().Children)
		DirectX::XMStoreFloat3x4(&*ptMat++, child.Transform);
	return localMatrices;
}

void Renderer::refreshMaterialResources()
{
	constexpr auto flags = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	faceAttributeBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, pFaceAttrTempBackBuffers[currentBackBufferIndex],
		scene->getFaceAttributes().data(), sizeof(FaceAttributes) * scene->getFaceAttributes().size(), flags);
	faceAttributeBuffer->SetName(L"Face Attribute Buffer");
	rendererResources.faceAttributeBuffer = faceAttributeBuffer;

	const auto& lights = scene->getLights().empty() ? vector<AreaLight>(1ULL) : scene->getLights();
	lightBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, pLightsTempBackBuffers[currentBackBufferIndex],
		lights.data(), sizeof(AreaLight) * lights.size(), flags);
	lightBuffer->SetName(L"Light Buffer");
	rendererResources.lightBuffer = lightBuffer;

	const auto& specs = scene->getSpeculars().empty() ? vector<SpecularPrimitive>(1ULL) : scene->getSpeculars();
	specularBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, pSpecularsTempBackBuffers[currentBackBufferIndex],
		specs.data(), sizeof(SpecularPrimitive) * specs.size(), flags);
	specularBuffer->SetName(L"Specular Buffer");
	rendererResources.specularBuffer = specularBuffer;
}

LRESULT Renderer::wndCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_SIZE:
		windowDimensions.x = LOWORD(lParam);
		windowDimensions.y = HIWORD(lParam);
		rendererResources.winDimensions = windowDimensions;
		resize();
		return true; // need to return not zero since app is filtering messages
	}
	return false;
}
