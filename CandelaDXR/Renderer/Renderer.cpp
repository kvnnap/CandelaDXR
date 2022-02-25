#include "Renderer.h"

#include "Exception/WindowException.h"
#include "Mathematics/Types.h"

#include "DirectX/DxUtil.h"
#include "DirectX/d3dx12.h"
#include <DirectXMath.h>

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
using candela::renderer::Renderer;
using candela::renderer::Camera;
using candela::renderer::IDrawable;
using candela::renderer::imgui::ImGuiSceneNode;

using DirectX::XMVectorSet;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Renderer::Renderer(Scene *scene, Camera *camera, const UVector2 &windowDimensions, vector<IDrawable*> p_drawables, uint32_t adapterIndex, bool debugEnabled, bool breakEnabled)
	: windowDimensions(windowDimensions),
	  adapterIndex(adapterIndex),
	  rtvDescriptorSize(),
	  currentBackBufferIndex(),
	  frameFenceValues(),
	  scene(scene),
	  camera(camera),
	  drawables(std::move(p_drawables)),
	  debugEnabled(debugEnabled),
	  breakEnabled(breakEnabled)
{
}

Renderer::~Renderer()
{
	if (commandQueue)
		commandQueue->flush();
	if (dxgiInfoManager && dxgiInfoManager->hasMessages())
	{
		std::cout << "Printing messages from IDXGIInfoQueue:" << std::endl;
		for (const auto& msg : dxgiInfoManager->getMessages())
			std::cout << msg << std::endl;
	}
}

void Renderer::init()
{
	window = make_unique<Window>("CandelaDXR", windowDimensions.x, windowDimensions.y, &keyboard, &mouse);

	// Init DirectX Debugging
	if (debugEnabled)
	{
		dxgiInfoManager = make_unique<directx::DxgiInfoManager>();
		DXUtil::enableDebugLayer(); // Not sure what happens if called twice
	}

	auto dxgiFactory = DXUtil::createDXGIFactory(debugEnabled);

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

	// Create render target Views
	rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto backBuffers = DXUtil::createRenderTargetViews(pDevice, pRTVDescriptorHeap, pSwapChain, NumBackBuffers);
	for (int i = 0; i < backBuffers.size(); ++i)
		pRTVBackBuffers[i] = backBuffers[i];

	// Upload scene resources
	initSceneResources();

	// ImGui
	pImGuiDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, 1u, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(window->getHandle());
	ImGui_ImplDX12_Init(pDevice.Get(), NumBackBuffers, DXGI_FORMAT_R8G8B8A8_UNORM, pImGuiDescriptorHeap.Get(),
		pImGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		pImGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	ImGui::StyleColorsDark();
	window->addWndProcCallback(ImGui_ImplWin32_WndProcHandler);
	for (auto& child : scene->getSceneGraph().Children)
		imguiSceneNodes.emplace_back(child, *scene);

	// Prepare struct to share with drawables
	rendererResources = RendererResources
	{
		.pDevice = pDevice,
		.sceneBuffer = sceneBuffer,
		.materialBuffer = materialBuffer,
		.faceAttributeBuffer = faceAttributeBuffer,
		.lightBuffer = lightBuffer,
		.matrices = matrices,
		.textures = textures,
		.pRTVDescriptorHeap = pRTVDescriptorHeap,
		.pRTVBackBuffers = backBuffers,
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
	auto rtvBackBuffer = pRTVBackBuffers[currentBackBufferIndex];

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(rtvBackBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	pCurrentCommandList = commandQueue->getCommandList();
	pCurrentCommandList->ResourceBarrier(1, &barrier);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), currentBackBufferIndex, rtvDescriptorSize);
	FLOAT color[] = { 0.f, 0.f, 0.f, 1.0f };
	pCurrentCommandList->ClearRenderTargetView(rtvDescriptorHandle, color, 0, nullptr);

	// ImGui
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Transforms");
	bool transformChanged = false;
	for (auto& imguiSceneNode : imguiSceneNodes)
	{
		imguiSceneNode.drawUi();
		transformChanged |= imguiSceneNode.hasChanged();
	}
	ImGui::End();

	ImGui::Render();

	// Update Transforms
	if (transformChanged) {
		auto gMatrices = getMatrices();
		DXUtil::updateDataInDefaultHeap(
			pDevice,
			pCurrentCommandList,
			matrices,
			pMatricesTempBackBuffers[currentBackBufferIndex],
			getMatrices().data(),
			sizeof(decltype(gMatrices)::value_type) * gMatrices.size(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	// Draw
	updateCamera();
	for (IDrawable* drawable : drawables)
	{
		if (transformChanged)
			drawable->onChange(pCurrentCommandList, currentBackBufferIndex);
		drawable->draw(pCurrentCommandList, currentBackBufferIndex);
	}

	// ImGui Render
	pCurrentCommandList->OMSetRenderTargets(1u, &rtvDescriptorHandle, FALSE, nullptr);
	pCurrentCommandList->SetDescriptorHeaps(1u, pImGuiDescriptorHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCurrentCommandList.Get());

	// End frame
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(rtvBackBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	pCurrentCommandList->ResourceBarrier(1, &barrier);
	frameFenceValues[currentBackBufferIndex] = commandQueue->executeCommandList(pCurrentCommandList);
	pCurrentCommandList.Reset();

	HRESULT hr;
	GFXTHROWIFFAILED(pSwapChain->Present(1u, 0u));
	ComPtr<IDXGISwapChain3> pSwapChain3;
	GFXTHROWIFFAILED(pSwapChain.As(&pSwapChain3));
	currentBackBufferIndex = pSwapChain3->GetCurrentBackBufferIndex();
	commandQueue->waitForFenceValue(frameFenceValues[currentBackBufferIndex]);

	// Stats
	if (transformChanged || camera->hasChanged())
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

	wrl::ComPtr<ID3D12Resource> tempVB;
	sceneBuffer = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, totalSize, D3D12_RESOURCE_STATE_COPY_DEST);
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
	wrl::ComPtr<ID3D12Resource> tempFace;
	materialBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, tempFace,
		scene->getMaterials().data(), sizeof(Material) * scene->getMaterials().size(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	wrl::ComPtr<ID3D12Resource> tempFaceAttr;
	faceAttributeBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, tempFaceAttr,
		scene->getFaceAttributes().data(), sizeof(FaceAttributes) * scene->getFaceAttributes().size(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	wrl::ComPtr<ID3D12Resource> tempLight;
	lightBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, tempLight,
		scene->getLights().data(), sizeof(AreaLight) * scene->getLights().size(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Copy Matrices
	wrl::ComPtr<ID3D12Resource> tempMatrices;
	auto localMatrices = getMatrices();
	matrices = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, tempMatrices,
		localMatrices.data(), sizeof(DirectX::XMFLOAT3X4) * localMatrices.size(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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
			DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	}

	if (textures.empty())
		textures.push_back(DXUtil::createTextureCommittedResource(
			pDevice, D3D12_HEAP_TYPE_DEFAULT, 1, 1, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_NONE, DXGI_FORMAT_R8G8B8A8_UNORM));

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

vector<DirectX::XMFLOAT3X4> Renderer::getMatrices()
{
	vector<DirectX::XMFLOAT3X4> localMatrices(scene->getSceneGraph().Children.size());
	auto ptMat = localMatrices.begin();
	for (auto child : scene->getSceneGraph().Children)
		DirectX::XMStoreFloat3x4(&*ptMat++, child.Transform);
	return localMatrices;
}
