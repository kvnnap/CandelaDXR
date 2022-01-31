#include "Renderer.h"

#include "Exception/WindowException.h"
#include "Mathematics/Types.h"
#include "Renderer/RasterShading.h"

#include "DirectX/DxUtil.h"
#include "DirectX/d3dx12.h"
#include <DirectXMath.h>

using std::make_unique;
using std::to_string;

using feanor::io::Keyboard;
using feanor::io::Mouse;

using candela::directx::DXUtil;
using candela::directx::CommandQueue;

using candela::mathematics::Vector2;
using candela::mathematics::Vector3;

using candela::ui::Window;
using candela::scene::Scene;
using candela::scene::Material;
using candela::scene::FaceAttributes;
using candela::renderer::Renderer;
using candela::renderer::Camera;
using candela::renderer::RasterShading;

using DirectX::XMVectorSet;

Renderer::Renderer(Scene *scene)
	: rtvDescriptorSize(),
	  currentBackBufferIndex(),
	  frameFenceValues(),
	  scene(scene)
{
	window = make_unique<Window>("CandelaDXR", 800, 600, &keyboard, &mouse);

	// Init DirectX Debugging
	DXUtil::enableDebugLayer();

	// Get DX12 compatible hardware device - Adapter contains info about the actual device
	D3D_FEATURE_LEVEL featureLevel;
	auto adapter = DXUtil::getAdapterLatestFeatureLevel(&featureLevel);
	pDevice = DXUtil::createDeviceFromAdapter(adapter, featureLevel);

	// Enable debug messages in debug mode for this device
	DXUtil::setupDebugLayer(pDevice);

	// Create command queue, with command list and command allocators
	commandQueue = make_unique<CommandQueue>(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);

	// Create swap chain
	pSwapChain = DXUtil::createSwapChain(commandQueue->getCommandQueue(), window->getHandle(), NumBackBuffers);

	// Create descriptor heap for render target view
	pRTVDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, NumBackBuffers, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create render target Views
	rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto backBuffers = DXUtil::createRenderTargetViews(pDevice, pRTVDescriptorHeap, pSwapChain, NumBackBuffers);
	for (int i = 0; i < backBuffers.size(); ++i)
		pRTVBackBuffers[i] = backBuffers[i];

	// Init Camera
	camera = make_unique<Camera>(
		XMVectorSet(0.f, 1.f, 3.5f, 1.f),
		XMVectorSet(0.f, 0.f, -1.f, 0.f),
		(float)800 / 600,
		1.0f,
		1.f,
		10.f);

	// Upload scene resources
	initSceneResources();

	// Init shading stuff
	rasterShading = make_unique<RasterShading>(pDevice, *commandQueue.get(), *scene, sceneBuffer, materialBuffer, faceAttributeBuffer, NumBackBuffers, *camera);
}

Renderer::~Renderer()
{
	commandQueue->flush();
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

	// Draw
	updateCamera();
	rasterShading->draw(pCurrentCommandList, pRTVDescriptorHeap, currentBackBufferIndex);

	// End frame
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(rtvBackBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	pCurrentCommandList->ResourceBarrier(1, &barrier);
	frameFenceValues[currentBackBufferIndex] = commandQueue->executeCommandList(pCurrentCommandList);
	pCurrentCommandList.Reset();

	HRESULT hr;
	GFXTHROWIFFAILED(pSwapChain->Present(1u, 0u));
	currentBackBufferIndex = pSwapChain->GetCurrentBackBufferIndex();
	commandQueue->waitForFenceValue(frameFenceValues[currentBackBufferIndex]);

	// Stats
	if (fpsCounter.hitFrame())
		window->setWindowName("CandelaDXR - Frames: " + to_string(fpsCounter.getTotalFrames()) + " FPS: " + to_string(fpsCounter.getFramesPerSecond()));
}

void Renderer::initSceneResources()
{
	auto totalSize = scene->getVertices().size() * sizeof(Vector3)
		+ scene->getTextureCoords().size() * sizeof(Vector2)
		+ scene->getNormals().size() * sizeof(Vector3)
		+ scene->getIndices().size() * sizeof(int);

	wrl::ComPtr<ID3D12Resource> tempVB;
	sceneBuffer = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, totalSize, D3D12_RESOURCE_STATE_COPY_DEST);
	auto tempResource = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_UPLOAD, totalSize, D3D12_RESOURCE_STATE_GENERIC_READ);
	std::uint8_t* data;
	auto readRange = D3D12_RANGE(0, 0);
	tempResource->Map(0, &readRange, reinterpret_cast<void**>(&data));
	size_t offset = scene->getVertices().size() * sizeof(Vector3);
	memcpy(data, scene->getVertices().data(), offset);
	memcpy(data + offset, scene->getTextureCoords().data(), scene->getTextureCoords().size() * sizeof(Vector2));
	offset += scene->getTextureCoords().size() * sizeof(Vector2);
	memcpy(data + offset, scene->getNormals().data(), scene->getNormals().size() * sizeof(Vector3));
	offset += scene->getNormals().size() * sizeof(Vector3);
	memcpy(data + offset, scene->getIndices().data(), scene->getIndices().size() * sizeof(int));
	tempResource->Unmap(0, nullptr);
	pCurrentCommandList = commandQueue->getCommandList();
	pCurrentCommandList->CopyResource(sceneBuffer.Get(), tempResource.Get());
	

	// Upload materials and face attributes (in separate buffers otherwise we have to take care of alignment)
	wrl::ComPtr<ID3D12Resource> tempFace;
	materialBuffer = DXUtil::uploadDataToDefaultHeap(
		pDevice,
		pCurrentCommandList,
		tempFace,
		scene->getMaterials().data(),
		sizeof(Material) * scene->getMaterials().size(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	wrl::ComPtr<ID3D12Resource> tempFaceAttr;
	faceAttributeBuffer = DXUtil::uploadDataToDefaultHeap(
		pDevice,
		pCurrentCommandList,
		tempFaceAttr,
		scene->getFaceAttributes().data(),
		sizeof(FaceAttributes) * scene->getFaceAttributes().size(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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
	camera->incrementPositionAlongDirection(getValueIfPressed('D', -deltaUnits), getValueIfPressed('W', deltaUnits));
	camera->incrementPositionAlongDirection(getValueIfPressed('A', deltaUnits), getValueIfPressed('S', -deltaUnits));

	deltaUnits = fpsCounter.getLastFrameTime() / 1000.f;
	camera->incrementDirection(getValueIfPressed('L', -deltaUnits), getValueIfPressed('I', -deltaUnits));
	camera->incrementDirection(getValueIfPressed('J', deltaUnits), getValueIfPressed('K', deltaUnits));
}
