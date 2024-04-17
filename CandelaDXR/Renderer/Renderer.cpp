#include "Renderer.h"
#include "RadianceBuffer.h"

#include "Exception/WindowException.h"
#include "Mathematics/Types.h"
#include "Mathematics/Plane.h"

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

#include <algorithm>
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
using candela::directx::ProfileItem;

using candela::mathematics::Vector2;
using candela::mathematics::UVector2;
using candela::mathematics::Vector3;
using candela::mathematics::Plane;

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
	  pRTV8Bit(), pRTVRad(), pRTVDiff(), pRTVSpec(), pRadAccumulator(), pDiffAccumulator(), pSpecAccumulator(),
	  rtvDescriptorSize(),
	  currentBackBufferIndex(),
	  scene(scene),
	  camera(camera),
	  chain(),
	  drawables(std::move(p_drawables)),
	  ppParams({ ACC_TONEMAP | ACC_LINEARTOSRGB }),
	  debugEnabled(debugEnabled),
	  breakEnabled(breakEnabled),
	  vsync(vsync),
	  exitOnAnimationCompletion(exitOnAnimCompl),
	  shaderAccumulation(shaderAccumulation)
{
	eoDebug.setEnabled(false);
	drawables.push_back(&eoDebug);
	buffersToGrab.push_back("rad_acc"); // Default buffer to grab
}

Renderer::~Renderer()
{
	if (commandQueue)
		commandQueue->flush();

	// Cannot destroy swapchain in full screen mode
	if (pSwapChain)
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
	//camera->setAspectRatio(static_cast<float>(windowDimensions.x) / windowDimensions.y);
	windowDimensions.x = static_cast<uint32_t>(windowDimensions.y * camera->getAspectRatio());
	window = make_unique<Window>("CandelaDXR", windowDimensions.x, windowDimensions.y, &keyboard, &mouse, !isRecording());
	using namespace std::placeholders;
	window->addWndProcCallback(std::bind(&Renderer::wndCallback, this, _1, _2, _3, _4), 0);

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
		.externalLights = externalLights,
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
		.frameNumber = 0,
		.processedExternalLights = std::move(rendererResources.processedExternalLights)
	};

	initShaders();
	createShaderResources();

	pCurrentCommandList = commandQueue->getCommandList();

	// Queries
	for (UINT i = 0; i < NumBackBuffers; ++i)
		timeStampQuery[i].init(pDevice, rendererResources.resourceManager.get(), commandQueue, pCurrentCommandList);

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

	// Setup stuff if in recording mode
	if (isRecording())
		recordingChange();
}

void Renderer::renderFrame()
{
	// Clear frame and start frame
	pCurrentCommandList = commandQueue->getCommandList();
	pRTVBackBuffers[currentBackBufferIndex]->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// Initially points to 32-bit RTV
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), NumBackBuffers, rtvDescriptorSize);
	FLOAT color[] = { 0.f, 0.f, 0.f, 0.0f };
	
	const auto recordingInitialState = isRecording();
	bool recordingChanged = false;
	if (recordingInitialState)
	{
		imguiManager.setEnabled(false);
	}
	else
	{
		if (keyboard.hasKeyChanged('Q') && keyboard.isKeyPressed('Q'))
			imguiManager.setEnabled(!imguiManager.isEnabled());
		updateCamera();
	}
	
	// Read previous "resolve"d query
	auto& tsQuery = timeStampQuery[currentBackBufferIndex];
	tsQuery.load();

	ChangeEvent_t changeEvent = imguiManager.processChangeEvent();
	if (camera->hasChanged())
		changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::Camera);
	if (!shaderAccumulation)
		changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::Clear);

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
			for (auto& sceneAnimRec : scene->getAnimationRecords())
			{
				if (sceneAnimRec.Enabled)
				{
					for (auto& animPair : sceneAnimRec.AnimPair)
					{
						//auto animResult = animPair.Animation->animate(timeMs, animRec.transform->getCentrePosition());
						for (auto& node : animPair.Node)
							node->transform(animPair.Animation->animate(timeMs, node->getCentrePosition()));
					}
				}
			}
			changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::Transformation) | static_cast<ChangeEvent_t>(ChangeEvent::Animation);
		}

		if (animationSequencer.isEnabled())
		{
			animationSequencer.tick();
			recordingChanged |= animationSequencer.isCompleted();
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

		// Update Lights
		rendererResources.processedExternalLights = getTransformedExternalLights();
		const auto& eLights = rendererResources.processedExternalLights;
		if (!eLights.empty())
		{
			DXUtil::updateDataInDefaultHeap(
				pDevice,
				pCurrentCommandList,
				externalLights,
				getTempResource(),
				eLights.data(),
				sizeof(candela::scene::Light) * eLights.size(),
				flags,
				flags);
		}

		// Update camera
		for (auto& cam : scene->getCameras())
		{
			if (cam.Camera.getName() == camera->getName())
			{
				camera->transform(cam.Node->getTransform());
				changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::Camera);
				break;
			}
		}
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

	// Check which frame to save, if any
	const bool needToGrab = std::binary_search(frameNumbersForGrab.begin(), frameNumbersForGrab.end(), fpsCounter.getFrameCount() + 1);
	if (needToGrab && frameNumbersForGrab.back() == fpsCounter.getFrameCount() + 1)
	{
		frameNumbersForGrab.clear();
		recordingChanged = true;
	}

	const auto grabRadiance = chain != nullptr && !buffersToGrab.empty() && (!recordingInitialState && keyboard.hasKeyChanged('P') && keyboard.isKeyPressed('P') || (animationSequencer.isEnabled() && animationSequencer.isNewFrame()) || needToGrab);
	const auto& dim = windowDimensions;

	tsQuery.addTimeStampQuery(pCurrentCommandList, "Begin");

	bool isFirst = true;
	for (size_t i = 0; i < drawables.size(); ++i)
	{
		auto drawable = drawables[i];
		if (changeEvent)
			drawable->onChange(pCurrentCommandList, currentBackBufferIndex, changeEvent);
		if (drawable->isEnabled())
		{
			const uint32_t buffUsage = isFirst ? 0xFF : drawable->getBufferUsage();

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
			flags |= isFirst || drawable->shouldClearAccumulation() ? ACC_CLEAR : ACC_NONE;
			flags |= ACC_ACCUMULATE;
			AccumConstBuff c32Data{};
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
			isFirst = false;
		}

		tsQuery.addTimeStampQuery(pCurrentCommandList, drawable->getName());
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
		if (!grabRadiance)
		{
			c32Data = { {AccumResource::RadAccumulator}, {AccumResource::RadAccumulator}, 1U, ppParams.Flags, ppParams.Exposure };
			pCurrentCommandList->SetComputeRoot32BitConstants(0u, sizeof(AccumConstBuff) / sizeof(uint32_t), &c32Data, 0);
			pCurrentCommandList->Dispatch(dim.x / 8 + (dim.x % 8 == 0 ? 0 : 1), dim.y / 8 + (dim.y % 8 == 0 ? 0 : 1), 1);
			pRadAccumulator->uavBarrier(pCurrentCommandList);
		}
	}

	// Extract radiance values if needed
	if (grabRadiance)
	{
		// Execute prev command list - command lists in same queue are executed in order
		commandQueue->executeCommandList(pCurrentCommandList);

		for (const auto& bufferToGrab : buffersToGrab)
		{
			auto buffer = rendererResources.resourceManager->getNamedResource(bufferToGrab);

			// Will cause synchronous behaviour (blocking)
			RadianceBuffer radBuffer = buffer->read(commandQueue);

			// Execute Chain to output data
			for (auto& chainItem : *chain)
				chainItem->process(radBuffer);
		}

		// Get another command list
		pCurrentCommandList = commandQueue->getCommandList();

		// Restore compute stuff
		bindComputePipeline();

		// Perform tone mapping
		AccumConstBuff c32Data{ {AccumResource::RadAccumulator}, {AccumResource::RadAccumulator}, 1U, ppParams.Flags, ppParams.Exposure };
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
	if (!isFirst)
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

	// Measure copy stuff
	tsQuery.addTimeStampQuery(pCurrentCommandList, "TM and RTV Copies");

	// ImGui Render
	imguiManager.draw(pCurrentCommandList, currentBackBufferIndex);

	// Measure ImGui stuff
	tsQuery.addTimeStampQuery(pCurrentCommandList, "ImGui");

	// Add query resolve to command list
	tsQuery.resolve(pCurrentCommandList);

	// End frame
	pRTVBackBuffers[currentBackBufferIndex]->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_PRESENT);
	frameFenceValues[currentBackBufferIndex] = commandQueue->executeCommandList(pCurrentCommandList);
	pCurrentCommandList.Reset();

	// Process any other resource dumps here (synch) TODO: do it async
	auto resourceToSave = imguiManager.getResourceToSave();
	if (resourceToSave && chain != nullptr)
	{
		// Will cause synchronous behaviour (blocking)
		RadianceBuffer radBuffer = resourceToSave->read(commandQueue);

		// Execute Chain to output data
		for (auto& chainItem : *chain)
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

	// Process Recording change
	if (recordingChanged)
		recordingChange();

	// Run post-frame actions
	while (!postFrameActions.empty())
	{
		postFrameActions.front()();
		postFrameActions.pop();
	}
}

void Renderer::setShaderAccumulation(bool p_shaderAccumulation)
{
	shaderAccumulation = p_shaderAccumulation;
}

bool Renderer::getShaderAccumulation() const
{
	return shaderAccumulation;
}

const Renderer::PostProcParams& Renderer::getPostProcParams() const
{
	return ppParams;
}

void Renderer::setPostProcParams(const PostProcParams& p_ppParams)
{
	ppParams = p_ppParams;
}

const vector<ProfileItem>& Renderer::getProfilingData() const
{
	return timeStampQuery[currentBackBufferIndex].getLoadedItems();
}

void Renderer::setCameraCopy(const Camera& p_camera)
{
	postFrameActions.push([&, p_camera] () {
		*camera = p_camera;
		camera->setChanged();
		window->setAspectRatio(camera->getAspectRatio());
	});
	
	//int width, height;
	//window->getClientWindowSize(width, height);
	//camera->setAspectRatio(static_cast<float>(width) / height);
}

const Scene& Renderer::getScene() const
{
	return *scene;
}

void Renderer::setChain(chain::CFList* chain)
{
	this->chain = chain;
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

void Renderer::setFramesToGrab(std::vector<std::uint64_t>&& framesToGrab)
{
	this->frameNumbersForGrab = std::move(framesToGrab);
}

void Renderer::setBuffersToGrab(std::vector<std::string>&& buffersToGrab)
{
	this->buffersToGrab = std::move(buffersToGrab);
}

void Renderer::initSceneResources()
{
	auto totalSize = scene->getVerticesSizeBytes() + scene->getTextureCoordsSizeBytes()
				   + scene->getNormalsSizeBytes() + scene->getIndicesSizeBytes();
	if (totalSize == 0 || scene->getFaceAttributes().empty())
		ThrowException("Scene is empty - nothing to render");

	wrl::ComPtr<ID3D12Resource> tempVB;
	auto tempResource = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_UPLOAD, totalSize, D3D12_RESOURCE_STATE_GENERIC_READ);
	uint8_t* data{};
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

	// Copy lights
	rendererResources.processedExternalLights = getTransformedExternalLights();
	const auto& eLights = rendererResources.processedExternalLights;
	const auto& eLightsTemp = eLights.empty() ? std::vector<candela::scene::Light>(1ULL) : eLights;
	wrl::ComPtr<ID3D12Resource> tempELight;
	externalLights = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, tempELight,
		eLightsTemp.data(), sizeof(std::vector<candela::scene::Light>::value_type) * eLightsTemp.size(), flags);
	externalLights->SetName(L"External Lights");

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
			texture->getBitsPerPixel(),
			texture->getTextureDXGIFormat(), flags), flags);
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
	CD3DX12_ROOT_PARAMETER1 param{};
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

	if (camera->hasChanged())
		return;

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

	// This disassociates the renderer camera so that further transforms do not affect this camera
	if (camera->hasChanged())
		camera->setName("_Candela_Default_Camera_");
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

vector<candela::scene::Light> Renderer::getTransformedExternalLights()
{
	//vector<Vector3> vertices; // DEbug
	using namespace DirectX;
	vector<scene::Light> tempLights;
	tempLights.reserve(scene->getExternalLights().size());
	for (auto& light : scene->getExternalLights())
	{
		auto& myLight = tempLights.emplace_back();
		auto transform = light.Node->getTransform();
		auto dirTrans = XMMatrixTranspose(XMMatrixInverse(nullptr, transform));

		myLight = light.Light;
		if (myLight.Type == LT_POINT)
		{
			myLight.Position = XMVector3Transform(myLight.Position, transform);
		}
		else if (myLight.Type == LT_SPOT)
		{
			myLight.Position = XMVector3Transform(myLight.Position, transform);
			myLight.Direction = XMVector4Transform(XMVector3Normalize(myLight.Direction), dirTrans);
		}
		else if (myLight.Type == LT_DIRECTIONAL)
		{
			// Get scene boundaries
			constexpr float deltaS = 0.01f;
			mathematics::Vector delta{ deltaS, deltaS, deltaS, 0.f };
			myLight.Direction = XMVector3Normalize(XMVector4Transform(XMVector3Normalize(myLight.Direction), dirTrans));

			const auto aabb = scene->getSceneAABB();
			auto closestPoint = aabb.getClosestToDirection(-myLight.Direction);

			// Create plane
			Plane plane (closestPoint, myLight.Direction);
			myLight.Up = plane.Basis.V;
			myLight.Right = plane.Basis.U;
			mathematics::AABB planeAABB;

			for (size_t i = 0; i < 8; ++i)
			{
				auto cornerPoint = aabb.getCornerPoint(i);
				auto uvPoint = plane.projectPointInUVSpace(cornerPoint);
				planeAABB.contain(uvPoint);
			}

			myLight.Position = XMVectorMultiplyAdd(delta, -myLight.Direction, plane.pointFromUV(planeAABB.Min - delta));
			XMStoreFloat2(&myLight.AreaDimensions, planeAABB.getDimensions() + delta * 2.f);
		}
		else if (myLight.Type == LT_AREA)
		{
			myLight.Position = XMVector3Transform(myLight.Position, transform);
			myLight.Direction = XMVector4Transform(XMVector3Normalize(myLight.Direction), dirTrans);
			myLight.Up = XMVector4Transform(XMVector3Normalize(myLight.Up), dirTrans);
			auto xDir = XMVector3Cross(myLight.Direction, myLight.Up);

			myLight.AreaDimensions = Vector2(
				myLight.AreaDimensions.x * XMVector3Length(xDir).m128_f32[0],
				myLight.AreaDimensions.y * XMVector3Length(myLight.Up).m128_f32[0]
			); // TODO: Check if this actually works

			myLight.Direction = XMVector3Normalize(myLight.Direction);
			myLight.Up = XMVector3Normalize(myLight.Up);
		}

		myLight.Position.m128_f32[3] = 1.f;
		myLight.Direction.m128_f32[3] = 0.f;
		myLight.Up.m128_f32[3] = 0.f;
	}
	//eoDebug.setVertices(std::move(vertices));
	return tempLights;
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

bool Renderer::isRecording() const
{
	return animationSequencer.isEnabled() || !frameNumbersForGrab.empty();
}

void Renderer::recordingChange()
{
	if (isRecording())
	{
		dxgiFactory->MakeWindowAssociation(window->getHandle(), DXGI_MWA_NO_ALT_ENTER);
	}
	else 
	{
		//  && animationSequencer.isCompleted()
		if (exitOnAnimationCompletion)
			PostQuitMessage(0);
		dxgiFactory->MakeWindowAssociation(window->getHandle(), 0);
	}
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
