#include "Renderer.h"

#include "Exception/WindowException.h"

#include "DirectX/DxUtil.h"
#include "DirectX/d3dx12.h"

using std::make_unique;
using std::to_string;

using feanor::io::Keyboard;
using feanor::io::Mouse;

using candela::directx::DXUtil;
using candela::directx::CommandQueue;

using candela::ui::Window;
using candela::renderer::Renderer;

Renderer::Renderer()
	: rtvDescriptorSize(),
	  currentBackBufferIndex(),
	  frameFenceValues()
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
