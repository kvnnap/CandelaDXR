#include "Renderer.h"
#include "RadianceBuffer.h"

#include "Exception/WindowException.h"
#include "Mathematics/Types.h"

#include "DirectX/DxUtil.h"
#include "DirectX/d3dx12.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>
#include <functional>

#include "DirectX/ShadingTable.h"

#include "System/DllManager.h"

#include "Chain/ToneMapping.h"
#include "Chain/AlphaCorrection.h"
#include "Chain/FileOutput.h"

#include "Shader/AccumulatorShader.hlsli"

#include <iostream>

using std::unique_ptr;
using std::make_unique;
using std::make_shared;
using std::to_string;
using std::vector;
using std::uint8_t;
using std::uint32_t;
using std::cout;
using std::endl;

using Microsoft::WRL::ComPtr;

using feanor::io::Keyboard;
using feanor::io::Mouse;

using candela::chain::ToneMapping;
using candela::chain::AlphaCorrection;
using candela::chain::FileOutput;

using candela::directx::DXUtil;
using candela::directx::CommandQueue;
using candela::directx::RootSignatureManager;
using candela::directx::DescriptorHeap;
using candela::directx::Resource;
using candela::directx::ResourceManager;

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
using candela::renderer::RadianceBuffer;
using candela::renderer::Camera;
using candela::renderer::IDrawable;
using candela::renderer::ChangeEvent;
using candela::renderer::ChangeEvent_t;
using candela::renderer::RendererTime;
using candela::renderer::AnimationRecord;
using candela::animation::AnimationSequencer;

using DirectX::XMVectorSet;

Renderer::Renderer(Scene *scene, Camera *camera, const UVector2 &windowDimensions, vector<IDrawable*> p_drawables,
	uint32_t adapterIndex, bool debugEnabled, bool breakEnabled, bool vsync, bool exitOnAnimCompl, bool shaderAccumulation)
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
	  exitOnAnimationCompletion(exitOnAnimCompl),
	  shaderAccumulation(shaderAccumulation)
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
		cout << "Printing messages from IDXGIInfoQueue:" << endl;
		for (const auto& msg : dxgiInfoManager->getMessages())
			cout << msg << endl;
	}
}

void Renderer::init()
{
	camera->setAspectRatio(static_cast<float>(windowDimensions.x) / windowDimensions.y);
	window = make_unique<Window>("CandelaDXR", windowDimensions.x, windowDimensions.y, &keyboard, &mouse, !animationSequencer.isEnabled());
	using namespace std::placeholders;
	window->addWndProcCallback(std::bind(&Renderer::wndCallback, this, _1, _2, _3, _4), 0);

	// Chains - TODO: Configurable through Factory
	chain.clear();
	auto fileOutput = make_unique<FileOutput>();
	//fileOutput->setFileType(FileOutput::RAW);
	//chain.push_back(std::move(fileOutput));
	chain.push_back(make_unique<ToneMapping>());
	chain.push_back(make_unique<AlphaCorrection>());
	fileOutput = make_unique<FileOutput>();
	fileOutput->setFileType(FileOutput::PNG);
	chain.push_back(std::move(fileOutput));

	// Allocate 
	pRTVBackBuffers.resize(NumBackBuffers);
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

	// Create descriptor heap for render target view - Num + pRTVRad, pRTVDiff, PRTVSpec
	pRTVDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, NumBackBuffers + 3, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	pRTVDescriptorHeap->SetName(L"RTV Descriptor Heap");

	// Other tex
	rendererResources.resourceManager = make_unique<ResourceManager>(pDevice);
	rendererResources.resourceManager->setTempBufferSlots(NumBackBuffers);
	auto& rMan = rendererResources.resourceManager;
	// The resource that will be used to copy back to render target
	pRTV8Bit = &rMan->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		windowDimensions.x, windowDimensions.y, DXGI_FORMAT_R8G8B8A8_UNORM, true);
	pRTV8Bit->setName(L"RTV Radiance 8-bit");

	// Create Float RTV Targets
	pRTVRad = &rMan->createResource(D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		windowDimensions.x, windowDimensions.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true);
	pRTVRad->setName(L"pRTVRad");
	pRTVDiff = &rMan->createResource(D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		windowDimensions.x, windowDimensions.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true);
	pRTVDiff->setName(L"pRTVDiff");
	pRTVSpec = &rMan->createResource(D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		windowDimensions.x, windowDimensions.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true);
	pRTVSpec->setName(L"pRTVSpec");

	// Create render target Views
	rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	pRTVBackBuffers = DXUtil::createRenderTargetViewsEx(pDevice, pRTVDescriptorHeap, pSwapChain, { pRTVRad, pRTVDiff, pRTVSpec }, NumBackBuffers);
	
	// Upload scene resources
	initSceneResources();

	// Prepare struct to share with drawables
	rendererResources = RendererResources
	{
		.renderer = this,
		.pDevice = pDevice,
		.sceneBuffer = sceneBuffer,
		.materialBuffer = materialBuffer,
		.faceAttributeBuffer = faceAttributeBuffer,
		.lightBuffer = lightBuffer,
		.specularBuffer = specularBuffer,
		.matrices = matrices,
		.normalMatrices = normalMatrices,
		.adapter = adapter,
		.textures = textures,
		.pRTVDescriptorHeap = pRTVDescriptorHeap,
		.pRTVRad = pRTVRad,
		.pRTVDiff = pRTVDiff,
		.pRTVSpec = pRTVSpec,
		.commandQueue = commandQueue.get(),
		.winDimensions = windowDimensions,
		.numBackBuffers = NumBackBuffers,
		.scene = scene,
		.camera = camera,
		.accelerationStructure = nullptr,
		.currentBackBufferIndex = 0,
		.resourceManager = std::move(rendererResources.resourceManager),
		.window = window.get(),
		.drawables = &drawables,
		.frameNumber = 0
	};

	initShaders();
	createShaderResources();

	pCurrentCommandList = commandQueue->getCommandList();

	// Init Resources
	for (auto &resource : resources)
		resource->init(&rendererResources, pCurrentCommandList);

	// Init and add those added later
	ResourceRegFunction resourceFn = [this] (std::unique_ptr<IResource> resource) -> void {
		resource->init(&rendererResources, pCurrentCommandList);
		resources.push_back(std::move(resource));
	};

	// Init drawables
	for (IDrawable* drawable : drawables)
		drawable->init(&rendererResources, pCurrentCommandList, resourceFn);
	imguiManager.init(&rendererResources, pCurrentCommandList, resourceFn);

	// Wait
	auto fV = commandQueue->executeCommandList(pCurrentCommandList);
	commandQueue->waitForFenceValue(fV);

	// Clear temporary buffers
	rendererResources.resourceManager->clearTemporaryBuffers(currentBackBufferIndex);

	// Anim
	if (animationSequencer.isEnabled())
		dxgiFactory->MakeWindowAssociation(window->getHandle(), DXGI_MWA_NO_ALT_ENTER);
}

void Renderer::renderFrame()
{
	// Clear frame and start frame
	pCurrentCommandList = commandQueue->getCommandList();
	pRTVBackBuffers[currentBackBufferIndex]->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// Initially points to 32-bit RTV
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), NumBackBuffers, rtvDescriptorSize);
	FLOAT color[] = { 0.f, 0.f, 0.f, 0.0f };

	// ImGui
	if (keyboard.hasKeyChanged('Q') && keyboard.isKeyPressed('Q'))
		imguiManager.setEnabled(!imguiManager.isEnabled());
	
	if (animationSequencer.isEnabled())
		imguiManager.setEnabled(false);
	else
		updateCamera();
	
	ChangeEvent_t changeEvent = imguiManager.processChangeEvent();
	changeEvent |= camera->hasChanged() ? static_cast<ChangeEvent_t>(ChangeEvent::Camera) : 0;

	constexpr auto flags = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	
	// Perform animations
	if (rendererTime.isRunning() || animationSequencer.isEnabled() || (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::Animation)))
	{
		bool needsAnimation{};
		if (animationSequencer.isEnabled())
		{
			rendererTime.stop();
			rendererTime.setElapsedTime(animationSequencer.getTimeMs());
			needsAnimation = animationSequencer.isNewFrame();
		}
		else
		{
			needsAnimation = rendererTime.isRunning() || (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::Animation));
		}

		if (needsAnimation)
		{
			auto timeMs = rendererTime.getTimeMs();
			for (auto& animRec : animationRecords)
				if (animRec.enabled)
					animRec.transform->transform(animRec.animation->animate(timeMs, animRec.transform->getCentrePosition()));
			changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::Transformation);
		}

		if (animationSequencer.isEnabled())
		{
			animationSequencer.tick();
			if (exitOnAnimationCompletion && animationSequencer.isCompleted())
				PostQuitMessage(0);
			if (!animationSequencer.isEnabled())
				dxgiFactory->MakeWindowAssociation(window->getHandle(), 0);
		}
	}

	// Update Transforms
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::Transformation))
	{
		auto gMatrices = getMatrices();
		DXUtil::updateDataInDefaultHeap(
			pDevice,
			pCurrentCommandList,
			matrices,
			getTempResource(),
			gMatrices.data(),
			sizeof(decltype(gMatrices)::value_type) * gMatrices.size(),
			flags,
			flags);

		auto nMatrices = getNormalMatrices();
		DXUtil::updateDataInDefaultHeap(
			pDevice,
			pCurrentCommandList,
			normalMatrices,
			getTempResource(),
			nMatrices.data(),
			sizeof(decltype(nMatrices)::value_type) * nMatrices.size(),
			flags,
			flags);
	}

	// Update material, face attributes and lights
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::SceneUpdate))
	{
		DXUtil::updateDataInDefaultHeap(pDevice, pCurrentCommandList, materialBuffer,
			getTempResource(), scene->getMaterials().data(),
			sizeof(Material) * scene->getMaterials().size(), flags, flags);
	}

	// On Scene change need to update lights, specs and face attributes
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::SceneChange))
	{
		scene->recalculateLightsAndFaceAttributes();
		commandQueue->flush(); // This ensures no resources are in the GPUs queue
		refreshMaterialResources();
	}

	// Resources on change
	for (auto& resource : resources)
		if (changeEvent)
			resource->onChange(pCurrentCommandList, currentBackBufferIndex, changeEvent);

	// Draw
	int32_t first = -1, last = -1;
	for (int32_t i = 0; i < static_cast<int>(drawables.size()); ++i)
	{
		if (!drawables[i]->isEnabled()) continue;
		if (first == -1)
			first = i;
		if (i > last)
			last = i;
	}

	const auto grabRadiancePressed = keyboard.hasKeyChanged('P') && keyboard.isKeyPressed('P') || (animationSequencer.isEnabled() && animationSequencer.isNewFrame());
	const auto& dim = windowDimensions;

	for (size_t i = 0; i < drawables.size(); ++i)
	{
		auto drawable = drawables[i];
		if (changeEvent || !shaderAccumulation)
			drawable->onChange(pCurrentCommandList, currentBackBufferIndex, changeEvent);
		if (!drawable->isEnabled())
			continue;

		uint32_t buffUsage = first == i ? 0xFF : drawable->getBufferUsage();

		// Clear the RadRTV 
		auto rtvDescHandle = rtvDescriptorHandle;
		for (uint32_t j = 0; j < 3; ++j)
		{
			if (buffUsage & (BufferUsage::Radiance << j)) // Clear the RadRTV, RTVDiff, RTVSpec
				pCurrentCommandList->ClearRenderTargetView(rtvDescHandle, color, 0, nullptr);
			rtvDescHandle.Offset(rtvDescriptorSize);
		}

		drawable->draw(pCurrentCommandList, currentBackBufferIndex);
		pRTVRad->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		pRTVDiff->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		pRTVSpec->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// Copy - per loop - testing here
		bindComputePipeline();
		uint32_t flags{};
		flags |= first == i || drawable->shouldClearAccumulation() ? ACC_CLEAR : ACC_NONE;
		flags |= ACC_ACCUMULATE;
		//flags |= !grabRadiancePressed && last == i ? ACC_TONEMAP | ACC_LINEARTOSRGB : ACC_NONE;
		AccumConstBuff c32Data;
		uint32_t bUsageIndex{}; // Accumulate rtvRad, rtvDiff and rtvSpec into their accumulators and optionnally clear.
		for (uint32_t bUI = 0; bUI < 3; ++bUI)
		{
			if (buffUsage & (BufferUsage::Radiance << bUI))
			{
				c32Data.InIndex[bUsageIndex] = static_cast<AccumResource>(AccumResource::RTVRad + bUI);
				c32Data.OutIndex[bUsageIndex] = static_cast<AccumResource>(AccumResource::RadAccumulator + bUI);
				++bUsageIndex;
			}
		}
		c32Data.PairCount = bUsageIndex;
		c32Data.Flags = flags;

		pCurrentCommandList->SetComputeRoot32BitConstants(0u, sizeof(AccumConstBuff) / sizeof(uint32_t), &c32Data, 0);
		pCurrentCommandList->Dispatch(dim.x / 8 + (dim.x % 8 == 0 ? 0 : 1), dim.y / 8 + (dim.y % 8 == 0 ? 0 : 1), 1);

		pRTVSpec->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pRTVDiff->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pRTVRad->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		pRadAccumulator->uavBarrier(pCurrentCommandList);
		pDiffAccumulator->uavBarrier(pCurrentCommandList);
		pSpecAccumulator->uavBarrier(pCurrentCommandList);
	}

	// Merge Spec and Diff towards Rad Accumulator
	bindComputePipeline();

	{
		// Accum Diff and Spec and Rad into RAD
		AccumConstBuff c32Data{ 
			{AccumResource::DiffAccumulator, AccumResource::SpecAccumulator}, 
			{AccumResource::RadAccumulator, AccumResource::RadAccumulator}, 2U, ACC_ACCUMULATE };
		pCurrentCommandList->SetComputeRoot32BitConstants(0u, sizeof(AccumConstBuff) / sizeof(uint32_t), &c32Data, 0);
		pCurrentCommandList->Dispatch(dim.x / 8 + (dim.x % 8 == 0 ? 0 : 1), dim.y / 8 + (dim.y % 8 == 0 ? 0 : 1), 1);

		pRadAccumulator->uavBarrier(pCurrentCommandList);

		// Tone map
		if (!grabRadiancePressed)
		{
			c32Data = { {AccumResource::RadAccumulator}, {AccumResource::RadAccumulator}, 1U, ACC_TONEMAP | ACC_LINEARTOSRGB };
			pCurrentCommandList->SetComputeRoot32BitConstants(0u, sizeof(AccumConstBuff) / sizeof(uint32_t), &c32Data, 0);
			pCurrentCommandList->Dispatch(dim.x / 8 + (dim.x % 8 == 0 ? 0 : 1), dim.y / 8 + (dim.y % 8 == 0 ? 0 : 1), 1);
			pRadAccumulator->uavBarrier(pCurrentCommandList);
		}
	}

	// Extract radiance values if needed
	if (grabRadiancePressed)
	{
		// Execute prev command list - command lists in same queue are executed in order
		commandQueue->executeCommandList(pCurrentCommandList);

		// Will cause synchronous behaviour (blocking)
		RadianceBuffer radBuffer = pRadAccumulator->read(commandQueue);

		// Execute Chain to output data
		for (auto& chainItem : chain)
			chainItem->process(radBuffer);

		// Get another command list
		pCurrentCommandList = commandQueue->getCommandList();

		// Restore compute stuff
		bindComputePipeline();

		// Perform tone mapping
		AccumConstBuff c32Data{ {AccumResource::RadAccumulator}, {AccumResource::RadAccumulator }, 1U, ACC_TONEMAP | ACC_LINEARTOSRGB };
		pCurrentCommandList->SetComputeRoot32BitConstants(0u, sizeof(AccumConstBuff) / sizeof(uint32_t), &c32Data, 0);
		pCurrentCommandList->Dispatch(dim.x / 8 + (dim.x % 8 == 0 ? 0 : 1), dim.y / 8 + (dim.y % 8 == 0 ? 0 : 1), 1);
		pRadAccumulator->uavBarrier(pCurrentCommandList);
	}

	// Copy accumulator to 8-bit Texture
	AccumConstBuff c32Data{ {AccumResource::RadAccumulator}, {AccumResource::RTV8BitBackBuffer}, 1U, ACC_CLEAR | ACC_ACCUMULATE };
	pCurrentCommandList->SetComputeRoot32BitConstants(0u, sizeof(AccumConstBuff) / sizeof(uint32_t), &c32Data, 0);
	pCurrentCommandList->Dispatch(dim.x / 8 + (dim.x % 8 == 0 ? 0 : 1), dim.y / 8 + (dim.y % 8 == 0 ? 0 : 1), 1);

	// Point to 8-bit swap chain buffers
	rtvDescriptorHandle.Offset(rtvDescriptorSize* (currentBackBufferIndex - NumBackBuffers));

	// Copy 8-bit Texture to swap-chain 8-bit back buffer
	if (first != -1)
	{
		pRTVBackBuffers[currentBackBufferIndex]->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
		pRTV8Bit->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);

		// Copy 8-bit tex to rtv
		pCurrentCommandList->CopyResource(*pRTVBackBuffers[currentBackBufferIndex], *pRTV8Bit);

		pRTV8Bit->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		pRTVBackBuffers[currentBackBufferIndex]->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
	else {
		pCurrentCommandList->ClearRenderTargetView(rtvDescriptorHandle, color, 0, nullptr);
	}

	// ImGui Render
	imguiManager.draw(pCurrentCommandList, currentBackBufferIndex);

	// End frame
	pRTVBackBuffers[currentBackBufferIndex]->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_PRESENT);
	frameFenceValues[currentBackBufferIndex] = commandQueue->executeCommandList(pCurrentCommandList);
	pCurrentCommandList.Reset();

	// Process any other resource dumps here (synch) TODO: do it async
	auto resourceToSave = imguiManager.getResourceToSave();
	if (resourceToSave)
	{
		// Will cause synchronous behaviour (blocking)
		RadianceBuffer radBuffer = resourceToSave->read(commandQueue);

		// Execute Chain to output data
		for (auto& chainItem : chain)
			chainItem->process(radBuffer);
	}

	HRESULT hr;
	// Present may wait on or execute the message pump when mode changes (fullscreen to windowed, etc)
	// Therefore, make sure RTV buffers are not referenced here
	GFXTHROWIFFAILED(pSwapChain->Present(vsync ? 1u : 0u, 0u));
	ComPtr<IDXGISwapChain3> pSwapChain3;
	GFXTHROWIFFAILED(pSwapChain.As(&pSwapChain3));
	rendererResources.currentBackBufferIndex = currentBackBufferIndex = pSwapChain3->GetCurrentBackBufferIndex();
	commandQueue->waitForFenceValue(frameFenceValues[currentBackBufferIndex]);
	rendererResources.resourceManager->clearTemporaryBuffers(currentBackBufferIndex);

	// Stats
	if (changeEvent)
		fpsCounter.resetFrameCount();
	if (fpsCounter.hitFrame())
		window->setWindowName("CandelaDXR - Frames: " + to_string(fpsCounter.getFrameCount()) + " FPS: " + to_string(fpsCounter.getFramesPerSecond()));

	// Reset camera
	camera->resetChanged();
	++rendererResources.frameNumber;
}

void Renderer::setShaderAccumulation(bool p_shaderAccumulation)
{
	shaderAccumulation = p_shaderAccumulation;
}

bool Renderer::getShaderAccumulation() const
{
	return shaderAccumulation;
}

void Renderer::setCameraCopy(const Camera& p_camera)
{
	*camera = p_camera;
	camera->setChanged();
}

const Scene& Renderer::getScene() const
{
	return *scene;
}

RendererTime& Renderer::getRendererTime()
{
	return rendererTime;
}

void Renderer::setAnimationRecords(vector<AnimationRecord>&& animationRecords)
{
	this->animationRecords = std::move(animationRecords);
}

vector<AnimationRecord>& Renderer::getAnimationRecords()
{
	return animationRecords;
}

AnimationSequencer& Renderer::getAnimationSequencer()
{
	return animationSequencer;
}

void Renderer::initSceneResources()
{
	auto totalSize = scene->getVerticesSizeBytes() + scene->getTextureCoordsSizeBytes()
				   + scene->getNormalsSizeBytes() + scene->getIndicesSizeBytes();
	if (totalSize == 0 || scene->getFaceAttributes().empty())
		ThrowException("Scene is empty - nothing to render");

	wrl::ComPtr<ID3D12Resource> tempVB;
	auto tempResource = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_UPLOAD, totalSize, D3D12_RESOURCE_STATE_GENERIC_READ);
	uint8_t* data;
	auto readRange = D3D12_RANGE(0, 0);
	tempResource->Map(0, &readRange, reinterpret_cast<void**>(&data));
	memcpy(data + scene->getVerticesOffset(), scene->getVertices().data(), scene->getVerticesSizeBytes());
	memcpy(data + scene->getTextureCoordsOffset(), scene->getTextureCoords().data(), scene->getTextureCoordsSizeBytes());
	memcpy(data + scene->getNormalsOffset(), scene->getNormals().data(), scene->getNormalsSizeBytes());
	memcpy(data + scene->getIndicesOffset(), scene->getIndices().data(), scene->getIndicesSizeBytes());
	tempResource->Unmap(0, nullptr);
	pCurrentCommandList = commandQueue->getCommandList();
	sceneBuffer = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, totalSize, D3D12_RESOURCE_STATE_COMMON);
	sceneBuffer->SetName(L"Scene Buffer");
	auto resBarrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(sceneBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	pCurrentCommandList->ResourceBarrier(1, &resBarrierDesc);
	pCurrentCommandList->CopyResource(sceneBuffer.Get(), tempResource.Get());

	// Upload materials and face attributes (in separate buffers otherwise we have to take care of alignment)
	constexpr auto flags = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	
	const auto& mats = scene->getMaterials().empty() ? vector<Material>(1ULL) : scene->getMaterials();
	materialBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList,
		getTempResource(), mats.data(), sizeof(Material) * mats.size(), flags);
	materialBuffer->SetName(L"Material Buffer");

	refreshMaterialResources();

	// Copy Matrices
	auto matVec = getMatrices();
	const auto& matrs = matVec.empty() ? decltype(matVec)(1ULL) : matVec;
	wrl::ComPtr<ID3D12Resource> tempMatrices;
	matrices = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, tempMatrices,
		matrs.data(), sizeof(decltype(matVec)::value_type) * matrs.size(), flags);
	matrices->SetName(L"Matrices Buffer");

	auto normMatVec = getNormalMatrices();
	const auto& normMatrs = normMatVec.empty() ? decltype(normMatVec)(1ULL) : normMatVec;
	wrl::ComPtr<ID3D12Resource> tempNormalMatrices;
	normalMatrices = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, tempNormalMatrices,
		normMatrs.data(), sizeof(decltype(normMatVec)::value_type) * normMatrs.size(), flags);
	normalMatrices->SetName(L"Normal Matrices Buffer");

	// Upload textures
	vector<wrl::ComPtr<ID3D12Resource>> texTempBuffer (scene->getTextures().size());
	auto tempTexBuffer = texTempBuffer.begin();
	for (const auto& texture : scene->getTextures())
	{
		textures.emplace_back(DXUtil::uploadTextureDataToDefaultHeap(
			pDevice,
			pCurrentCommandList,
			*tempTexBuffer++,
			texture->data(),
			texture->getWidth(),
			texture->getHeight(),
			texture->getChannels(),
			DXGI_FORMAT_R8G8B8A8_UNORM, flags), flags);
		textures.back().setName(L"Texture");
	}

	if (textures.empty())
	{
		textures.emplace_back(DXUtil::createTextureCommittedResource(
			pDevice, D3D12_HEAP_TYPE_DEFAULT, 1, 1, flags, D3D12_RESOURCE_FLAG_NONE, DXGI_FORMAT_R8G8B8A8_UNORM), flags);
		textures.back().setName(L"Empty Texture");
	}

	auto fenceValue = commandQueue->executeCommandList(pCurrentCommandList);
	commandQueue->waitForFenceValue(fenceValue);
}

void Renderer::initShaders()
{
	HRESULT hr;
	// Compute shader
	computeRSM = make_shared<RootSignatureManager>();
	CD3DX12_ROOT_PARAMETER1 param; 
	computeRSM->addDescriptorRange("ComputeData", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 7, 0));
	computeRSM->setDescriptorTableParameter("ComputeDataDescTable", "ComputeData");
	param.InitAsConstants(sizeof(AccumConstBuff) / sizeof(uint32_t), 0u);
	computeRSM->setParameter("ComputeConstants", param); // inIndex, outIndex, flags
	computeRSM->addParametersToRootSignature("ComputeRootSignature", { "ComputeConstants", "ComputeDataDescTable"});
	computeRootSignature = computeRSM->generateRootSignature("ComputeRootSignature", pDevice, D3D12_ROOT_SIGNATURE_FLAG_NONE);

	// Get shader
	wrl::ComPtr<ID3DBlob> pComputeBlob;
	GFXTHROWIFFAILED(D3DReadFileToBlob(L"./Shaders/AccumulatorShader.cso", &pComputeBlob));

	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_CS CS;
	} pipelineStateStream;

	pipelineStateStream.pRootSignature = computeRootSignature.Get();
	pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(pComputeBlob.Get());

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc =
	{
		sizeof(PipelineStateStream), &pipelineStateStream
	};
	ComPtr<ID3D12Device5> pDevice5;
	GFXTHROWIFFAILED(pDevice.As(&pDevice5));
	GFXTHROWIFFAILED(pDevice5->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&computePipelineState)));

	pRadAccumulator = &rendererResources.resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		windowDimensions.x, windowDimensions.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "rad_acc");
	pDiffAccumulator = &rendererResources.resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		windowDimensions.x, windowDimensions.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "diff_acc");
	pSpecAccumulator = &rendererResources.resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		windowDimensions.x, windowDimensions.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "spec_acc");
}

void Renderer::createShaderResources()
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	auto cmpDescHeapManager = DescriptorHeap(computeRSM, "ComputeDataDescTable", "ComputeData1", pDevice);
	cmpDescHeapManager.setUAV(0, uavDesc, pDevice, *pRTV8Bit);
	cmpDescHeapManager.setUAV(1, uavDesc, pDevice, *pRTVRad);
	cmpDescHeapManager.setUAV(2, uavDesc, pDevice, *pRTVDiff);
	cmpDescHeapManager.setUAV(3, uavDesc, pDevice, *pRTVSpec);
	cmpDescHeapManager.setUAV(4, uavDesc, pDevice, *pRadAccumulator);
	cmpDescHeapManager.setUAV(5, uavDesc, pDevice, *pDiffAccumulator);
	cmpDescHeapManager.setUAV(6, uavDesc, pDevice, *pSpecAccumulator);

	computeDescriptorHeap = cmpDescHeapManager.getDescriptorHeap();
}

void Renderer::updateCamera()
{
	auto getValueIfPressed = [this](char key, float value) {
		return keyboard.isKeyPressed(key) ? value : 0.f;
	};

	constexpr float unitsPerSec = 3.f;

	float deltaUnits = fpsCounter.getLastFrameTime() / 1000.f * unitsPerSec;
	if (keyboard.isKeyPressed('D') || keyboard.isKeyPressed('W'))
		camera->incrementPositionAlongDirection(getValueIfPressed('D', deltaUnits), getValueIfPressed('W', deltaUnits));
	if (keyboard.isKeyPressed('A') || keyboard.isKeyPressed('S'))
		camera->incrementPositionAlongDirection(getValueIfPressed('A', -deltaUnits), getValueIfPressed('S', -deltaUnits));

	deltaUnits = fpsCounter.getLastFrameTime() / 1000.f;
	if (keyboard.isKeyPressed('L') || keyboard.isKeyPressed('I'))
		camera->incrementDirection(getValueIfPressed('L', -deltaUnits), getValueIfPressed('I', deltaUnits));
	if (keyboard.isKeyPressed('J') || keyboard.isKeyPressed('K'))
		camera->incrementDirection(getValueIfPressed('J', deltaUnits), getValueIfPressed('K', -deltaUnits));
}

void Renderer::resize()
{
	// Wait for all GPU operations to complete
	commandQueue->flush();
	pRTVBackBuffers.clear();
	rendererResources.currentBackBufferIndex = currentBackBufferIndex = 0;
	camera->setAspectRatio(static_cast<float>(windowDimensions.x) / windowDimensions.y);
	HRESULT hr;
	auto flags = DXUtil::checkTearingSupport(dxgiFactory) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	GFXTHROWIFFAILED(pSwapChain->ResizeBuffers(NumBackBuffers, windowDimensions.x, windowDimensions.y, DXGI_FORMAT_UNKNOWN, flags));
	// Resize textures in resource manager
	rendererResources.resourceManager->resize(windowDimensions.x, windowDimensions.y);
	pRTVBackBuffers = DXUtil::createRenderTargetViewsEx(pDevice, pRTVDescriptorHeap, pSwapChain, { pRTVRad, pRTVDiff, pRTVSpec }, NumBackBuffers);
	fpsCounter.resetFrameCount();
	
	createShaderResources(); // some resources used in renderer depend on resizing
	for (IDrawable* drawable : drawables)
		drawable->onResize();
	imguiManager.onResize();
}

vector<DirectX::XMFLOAT3X4> Renderer::getMatrices()
{
	auto meshInstances = scene->getSceneGraph().getFlattenedMeshNodes();
	vector<DirectX::XMFLOAT3X4> localMatrices(meshInstances.size());
	auto ptMat = localMatrices.begin();
	for (const auto& child : meshInstances)
		DirectX::XMStoreFloat3x4(&*ptMat++, child.ComputedTransform); // Transpose implicit since we read as 4x3 in shader
	return localMatrices;
}

vector<DirectX::XMFLOAT3X3> Renderer::getNormalMatrices()
{
	auto meshInstances = scene->getSceneGraph().getFlattenedMeshNodes();
	vector<DirectX::XMFLOAT3X3> localMatrices(meshInstances.size());
	auto ptMat = localMatrices.begin();
	// Transpose needed for row to col major but it cancels with the tranpose we are supposed to apply
	for (const auto& child : meshInstances)
		DirectX::XMStoreFloat3x3(&*ptMat++, DirectX::XMMatrixInverse(nullptr, child.ComputedTransform));
	return localMatrices;
}

void Renderer::refreshMaterialResources()
{
	constexpr auto flags = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	faceAttributeBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, getTempResource(),
		scene->getFaceAttributes().data(), sizeof(FaceAttributes) * scene->getFaceAttributes().size(), flags);
	faceAttributeBuffer->SetName(L"Face Attribute Buffer");
	rendererResources.faceAttributeBuffer = faceAttributeBuffer;

	const auto& lights = scene->getLights().empty() ? vector<AreaLight>(1ULL) : scene->getLights();
	lightBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, getTempResource(),
		lights.data(), sizeof(AreaLight) * lights.size(), flags);
	lightBuffer->SetName(L"Light Buffer");
	rendererResources.lightBuffer = lightBuffer;

	const auto& specs = scene->getSpeculars().empty() ? vector<SpecularPrimitive>(1ULL) : scene->getSpeculars();
	specularBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, getTempResource(),
		specs.data(), sizeof(SpecularPrimitive) * specs.size(), flags);
	specularBuffer->SetName(L"Specular Buffer");
	rendererResources.specularBuffer = specularBuffer;
}

void Renderer::bindComputePipeline()
{
	pCurrentCommandList->SetPipelineState(computePipelineState.Get());
	pCurrentCommandList->SetComputeRootSignature(computeRootSignature.Get());
	pCurrentCommandList->SetDescriptorHeaps(1u, computeDescriptorHeap.GetAddressOf());
	pCurrentCommandList->SetComputeRootDescriptorTable(1u, computeDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
}

candela::directx::DXResource& Renderer::getTempResource()
{
	return rendererResources.getTempResource();
}

LRESULT Renderer::wndCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_SIZE:
		UVector2 dim = { LOWORD(lParam), HIWORD(lParam) };
		if (dim.x == 0 || dim.y == 0)
			return 1;
		rendererResources.winDimensions = windowDimensions = dim;
		resize();
		return 0;
	}
	return 1;
}
