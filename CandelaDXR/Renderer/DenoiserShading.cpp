#include "DenoiserShading.h"

#include <d3dcompiler.h>
#include "Exception/WindowException.h"
#include "Util/StringUtil.h"
#include "Mathematics/Types.h"

#include "NRIDescs.h"
#include "NRI.h"
#include "NRIDescs.hpp"
#include "Extensions/NRIHelper.h"
#include "NVIDIA/NRDIntegration.h"
#include "NVIDIA/NRDIntegration.hpp"
#include "Extensions/NRIWrapperD3D12.h"
#include "Extensions/NRIHelper.h"

#include <iostream>

using std::make_unique;
using std::make_shared;
using std::uint32_t;
using std::uint16_t;

using candela::renderer::DenoiserShading;
using candela::directx::RootSignatureManager;
using candela::directx::Resource;
using candela::directx::DescriptorHeap;
using candela::mathematics::UVector2;

//extern D3D12_RESOURCE_STATES GetResourceStates(nri::AccessBits accessMask);

namespace candela::renderer
{
	struct NriInterface
		: public nri::CoreInterface
		, public nri::HelperInterface
		, public nri::WrapperD3D12Interface
	{};
}

DenoiserShading::DenoiserShading()
	: rendererResources(), nriDevice(), rasterShader(true),
	  radAccumulator(), albedo(), normal(), depth(), pt_rad(),
	  in_mv(), in_normal_roughness(), in_view_z(),
	  in_diff_radiance_hitdist(), out_diff_radiance_hitdist()
{
}

DenoiserShading::~DenoiserShading()
{
	destroyDenoiser();
}

struct DenoiserResource
{
	Resource* resource;
	D3D12_RESOURCE_STATES requiredState;
	nrd::ResourceType resourceType;
	nri::Format format;
};

D3D12_RESOURCE_STATES GetResourceState(nri::AccessBits accessBits)
{
	D3D12_RESOURCE_STATES result{};
	if (accessBits & nri::AccessBits::SHADER_RESOURCE)
		result |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	if (accessBits & nri::AccessBits::SHADER_RESOURCE_STORAGE)
		result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	if (result == 0)
		throw std::runtime_error("Cannot convert nri::AccessBits");
	return result;
}

nri::AccessBits GetResourceState(D3D12_RESOURCE_STATES d3dResourceState)
{
	nri::AccessBits result{};
	if (d3dResourceState & D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
		result |= nri::AccessBits::SHADER_RESOURCE;
	if (d3dResourceState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		result |= nri::AccessBits::SHADER_RESOURCE_STORAGE;
	if (result == nri::AccessBits::UNKNOWN)
		throw std::runtime_error("Cannot convert D3D12_RESOURCE_STATES");
	return result;
}

void DenoiserShading::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	rendererResources = rRes;

	// Setup raster shader
	rasterShader.setGlobaResourcePrefix("den_");
	rasterShader.init(rRes, pCurrentCommandList, resRegFn);
	rasterShader.setComputeRadiance(false);

	// Get resources & create new ones
	auto& dim = rRes->winDimensions;
	radAccumulator = rRes->resourceManager->getNamedResource("diff_acc");
	albedo = rRes->resourceManager->getNamedResource("den_gAlb");
	normal = rRes->resourceManager->getNamedResource("den_gNorm");
	depth = rRes->resourceManager->getNamedResource("den_gDepth");
	pt_rad = rRes->resourceManager->getNamedResource("pt_diff");

	in_mv = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "in_mv");
	in_normal_roughness = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "in_normal_roughness");
	in_view_z = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32_FLOAT, true, "in_view_z");
	in_diff_radiance_hitdist = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "in_diff_radiance_hitdist");
	out_diff_radiance_hitdist = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "out_diff_radiance_hitdist");

	// Setup compute shader RS
	auto rsm = make_shared<RootSignatureManager>();
	CD3DX12_ROOT_PARAMETER1 param;
	rsm->addDescriptorRange("IORange", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6u, 0u));
	rsm->addDescriptorRange("IORange", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5u, 0u));
	rsm->setDescriptorTableParameter("IODescTable", "IORange");
	param.InitAsConstants(3u, 0u); rsm->setParameter("Constants", param);
	rsm->addParametersToRootSignature("ComputeRootSignature", { "IODescTable", "Constants" });
	computeRootSignature = rsm->generateRootSignature("ComputeRootSignature", rRes->pDevice, D3D12_ROOT_SIGNATURE_FLAG_NONE);

	// Heap
	descHeapManager = make_unique<DescriptorHeap>(rsm, "IODescTable", "IO1", rRes->pDevice);
	createShaderResources();
	
	// CS Pipeline
	// Load Shader
	HRESULT hr;
	wrl::ComPtr<ID3DBlob> pComputeBlob;
	GFXTHROWIFFAILED(D3DReadFileToBlob(util::StringToWString("./Shaders/DenoiserPassShader.cso").c_str(), &pComputeBlob));

	// Finally generate pipeline state
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
	wrl::ComPtr<ID3D12Device5> pDevice5;
	GFXTHROWIFFAILED(rendererResources->pDevice.As(&pDevice5));
	GFXTHROWIFFAILED(pDevice5->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&computePipelineState)));

	// NVIDIA stuff
	NRI = make_unique<NriInterface>();

	// --- NVIDIA Wrap device
	nri::DeviceCreationD3D12Desc deviceDesc = {};
	deviceDesc.d3d12Device = rRes->pDevice.Get();
	deviceDesc.d3d12PhysicalAdapter = rRes->adapter.Get();
	deviceDesc.d3d12GraphicsQueue = rRes->commandQueue->getCommandQueue().Get();
	deviceDesc.enableNRIValidation = false;

	// Wrap the device
	nri::Result result = nri::CreateDeviceFromD3D12Device(deviceDesc, nriDevice);

	// Get needed functionality
	result = nri::GetInterface(*nriDevice, NRI_INTERFACE(nri::CoreInterface), (nri::CoreInterface*)NRI.get());
	result = nri::GetInterface(*nriDevice, NRI_INTERFACE(nri::HelperInterface), (nri::HelperInterface*)NRI.get());

	// Get needed "wrapper" extension, XXX - can be D3D11, D3D12 or VULKAN
	result = nri::GetInterface(*nriDevice, NRI_INTERFACE(nri::WrapperD3D12Interface), (nri::WrapperD3D12Interface*)NRI.get());

	setupDenoiser();
	// --- END NVIDIA Wrap device
}

void DenoiserShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex)
{
	wrl::ComPtr<ID3D12DebugCommandList> pDbgCmdList;
	pCurrentCommandList.As(&pDbgCmdList);
	//pDbgCmdList->SetFeatureMask(D3D12_DEBUG_FEATURE_ALLOW_BEHAVIOR_CHANGING_DEBUG_AIDS);
	//bool isUAV = pDbgCmdList->AssertResourceState(*out_diff_radiance_hitdist, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	rasterShader.draw(pCurrentCommandList, currentBackBufferIndex);

	// Compute pre-pass
	radAccumulator->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	albedo->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	normal->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	depth->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	pt_rad->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	rendererResources->pRTVDiff->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);


	compute(pCurrentCommandList, 0);

	in_mv->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	in_normal_roughness->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	in_view_z->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	in_diff_radiance_hitdist->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

	/// Pass to convert radiance without albedo - compute shader required


	// Denoising ////////////////////////////////////////////////////

	nri::CommandBufferD3D12Desc cmdDesc = {};
	cmdDesc.d3d12CommandList = pCurrentCommandList.Get();
	cmdDesc.d3d12CommandAllocator = nullptr; // rendererResources->commandQueue->getCommandAllocator(pCurrentCommandList); // Not needed for NRD integration layer

	nri::CommandBuffer* cmdBuffer = nullptr;
	NRI->CreateCommandBufferD3D12(*nriDevice, cmdDesc, cmdBuffer);

	// Wrap required textures
	constexpr int N = 5;
	nri::TextureTransitionBarrierDesc entryDescs[N] = {};
	nri::Format entryFormat[N] = {};
	DenoiserResource denoiserResources[N] = {
		{ in_mv,                     D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nrd::ResourceType::IN_MV,                     nri::Format::RGBA32_SFLOAT },
		{ in_normal_roughness,       D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nrd::ResourceType::IN_NORMAL_ROUGHNESS,       nri::Format::RGBA32_SFLOAT },
		{ in_view_z,                 D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nrd::ResourceType::IN_VIEWZ,                  nri::Format::R32_SFLOAT    },
		{ in_diff_radiance_hitdist,  D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST,  nri::Format::RGBA32_SFLOAT },
		{ out_diff_radiance_hitdist, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,    nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, nri::Format::RGBA32_SFLOAT }
	};

	for (uint32_t i = 0; i < N; i++)
	{
		// You need to specify the current state of the resource here, after denoising NRD can modify
		// this state. Application must continue state tracking from this point.
		// Useful information:
		//    SRV = nri::AccessBits::SHADER_RESOURCE, nri::TextureLayout::SHADER_RESOURCE
		//    UAV = nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::TextureLayout::GENERAL
		auto& denResource = denoiserResources[i];
		nri::TextureD3D12Desc textureDesc = {};
		textureDesc.d3d12Resource = *denResource.resource;
		NRI->CreateTextureD3D12(*nriDevice, textureDesc, (nri::Texture*&)entryDescs[i].texture);
		entryDescs[i].nextAccess = GetResourceState(denResource.requiredState);
		entryDescs[i].nextLayout = entryDescs[i].nextAccess == nri::AccessBits::SHADER_RESOURCE ? nri::TextureLayout::SHADER_RESOURCE : nri::TextureLayout::GENERAL;
	}

	nrdCommonSettings.frameIndex = static_cast<uint32_t>(rendererResources->frameNumber);
	nrdCommonSettings.isMotionVectorInWorldSpace = true;

	auto camera = rendererResources->camera;
	// transform to column-Major
	auto viewMatrix = /*DirectX::XMMatrixTranspose*/(camera->getViewMatrix());
	auto persMatrix = /*DirectX::XMMatrixTranspose*/(camera->getPerspectiveMatrix());
	//if (camera->hasChanged())
	//{
	//	for (int i = 0; i < 4; ++i)
	//	{
	//		for (int j = 0; j < 4; ++j)
	//		{
	//			std::cout << viewMatrix.r[i].m128_f32[j] << ", ";
	//		}
	//		std::cout << std::endl;
	//	}
	//}


	if (nrdCommonSettings.frameIndex == 0)
	{
		memcpy(&nrdCommonSettings.worldToViewMatrixPrev, &viewMatrix, sizeof(nrdCommonSettings.worldToViewMatrixPrev));
		memcpy(&nrdCommonSettings.viewToClipMatrixPrev, &persMatrix, sizeof(nrdCommonSettings.viewToClipMatrixPrev));
	}
	else
	{
		memcpy(&nrdCommonSettings.worldToViewMatrixPrev, &nrdCommonSettings.worldToViewMatrix, sizeof(nrdCommonSettings.worldToViewMatrixPrev));
		memcpy(&nrdCommonSettings.viewToClipMatrixPrev, &nrdCommonSettings.viewToClipMatrix, sizeof(nrdCommonSettings.viewToClipMatrixPrev));
	}

	memcpy(&nrdCommonSettings.worldToViewMatrix, &viewMatrix, sizeof(nrdCommonSettings.worldToViewMatrix));
	memcpy(&nrdCommonSettings.viewToClipMatrix, &persMatrix, sizeof(nrdCommonSettings.viewToClipMatrix));

	
	//nrdReblurSettings.checkerboardMode = nrd::CheckerboardMode::WHITE;
	//nrdReblurSettings.diffusePrepassBlurRadius = 1.f;
	//nrdReblurSettings.blurRadius = .5f;
	//nrdReblurSettings.stabilizationStrength = 0.1f;
	//nrdReblurSettings.enableReferenceAccumulation = true;
	//nrdReblurSettings.historyFixFrameNum = 0;

	NRD->SetMethodSettings(nrd::Method::REBLUR_DIFFUSE, &nrdReblurSettings);
	
	// Populate the user pool
	NrdUserPool userPool = {};
	for (uint32_t i = 0; i < N; i++)
	{
		auto& denResource = denoiserResources[i];
		NrdIntegrationTexture tex{};
		tex.format = denResource.format;
		tex.subresourceStates = &entryDescs[i];
		NrdIntegration_SetResource(userPool, denResource.resourceType, tex);
	}
	
	NRD->Denoise(currentBackBufferIndex, *cmdBuffer, nrdCommonSettings, userPool, false);
	
	// Sync states
	for (uint32_t i = 0; i < N; i++)
	{
		auto& denResource = denoiserResources[i];
		auto reqState = GetResourceState(denResource.requiredState);
		auto reqLayout = reqState == nri::AccessBits::SHADER_RESOURCE ? nri::TextureLayout::SHADER_RESOURCE : nri::TextureLayout::GENERAL;
		if (entryDescs[i].nextAccess != reqState || entryDescs[i].nextLayout != reqLayout)
		{
			denResource.resource->rewriteState(GetResourceState(entryDescs[i].nextAccess));
			denResource.resource->transistionBarrier(pCurrentCommandList, denResource.requiredState);
		}
	}

	for (uint32_t i = 0; i < N; i++)
		NRI->DestroyTexture(*(nri::Texture*&)entryDescs[i].texture);
	NRI->DestroyCommandBuffer(*cmdBuffer);
	/////////////////////////////////////////////////////////////////
	
	// Post pass
	in_mv->transitionToPrevBarrier(pCurrentCommandList);
	in_normal_roughness->transitionToPrevBarrier(pCurrentCommandList);
	in_view_z->transitionToPrevBarrier(pCurrentCommandList);
	in_diff_radiance_hitdist->transitionToPrevBarrier(pCurrentCommandList);

	compute(pCurrentCommandList, 1);

	radAccumulator->transitionToPrevBarrier(pCurrentCommandList);
	albedo->transitionToPrevBarrier(pCurrentCommandList);
	normal->transitionToPrevBarrier(pCurrentCommandList);
	depth->transitionToPrevBarrier(pCurrentCommandList);
	pt_rad->transitionToPrevBarrier(pCurrentCommandList);
	rendererResources->pRTVDiff->transitionToPrevBarrier(pCurrentCommandList);

	nrdCommonSettings.accumulationMode = nrd::AccumulationMode::CONTINUE;

	//// Copy to output buffer
	//out_diff_radiance_hitdist->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	//rendererResources->pRTVRadBackBuffer->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
	//pCurrentCommandList->CopyResource(*rendererResources->pRTVRadBackBuffer, *out_diff_radiance_hitdist);
	//rendererResources->pRTVRadBackBuffer->transitionToPrevBarrier(pCurrentCommandList);
	//out_diff_radiance_hitdist->transitionToPrevBarrier(pCurrentCommandList);
}

void DenoiserShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	rasterShader.onChange(pCurrentCommandList, currentBackBufferIndex, changeEvent);
}

void DenoiserShading::onResize()
{
	rasterShader.onResize();
	createShaderResources();
	setupDenoiser();
}

void DenoiserShading::accept(IVisitor* visitor)
{
	visitor->visit(this);
}

bool DenoiserShading::shouldClearAccumulation() const
{
	return true;
}

nrd::CommonSettings& DenoiserShading::getCommonSettings()
{
	return nrdCommonSettings;
}

nrd::ReblurSettings& DenoiserShading::getReblurSettings()
{
	return nrdReblurSettings;
}

void DenoiserShading::clearHistory()
{
	nrdCommonSettings.accumulationMode = nrd::AccumulationMode::RESTART;
}

void DenoiserShading::compute(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t mode)
{
	out_diff_radiance_hitdist->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	pCurrentCommandList->SetComputeRootSignature(computeRootSignature.Get());
	pCurrentCommandList->SetPipelineState(computePipelineState.Get());
	pCurrentCommandList->SetDescriptorHeaps(1u, descHeapManager->getDescriptorHeap().GetAddressOf());
	pCurrentCommandList->SetComputeRootDescriptorTable(0u, descHeapManager->getDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	auto camDim = rendererResources->camera->getNearPlaneDimensions();
	pCurrentCommandList->SetComputeRoot32BitConstants(1u, 2u, &camDim.m128_f32[2], 0u);
	pCurrentCommandList->SetComputeRoot32BitConstant(1u, mode, 2u);
	constexpr auto ThreadGroupDim = 8u;
	auto& dim = rendererResources->winDimensions;
	auto launchDimensions = UVector2(dim.x / ThreadGroupDim + (dim.x % ThreadGroupDim == 0u ? 0u : 1u), dim.y / ThreadGroupDim + (dim.y % ThreadGroupDim == 0u ? 0u : 1u));
	pCurrentCommandList->Dispatch(launchDimensions.x, launchDimensions.y, 1u);

	out_diff_radiance_hitdist->transitionToPrevBarrier(pCurrentCommandList);
}

void DenoiserShading::createShaderResources()
{
	auto rRes = rendererResources;

	// Bind res
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.Texture2D.MipLevels = 1u;
	uint32_t entryNum{};
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *radAccumulator);
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *albedo);
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *normal);
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *depth);
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *pt_rad);
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *out_diff_radiance_hitdist);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_mv);
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_normal_roughness);

	uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_view_z);

	uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_diff_radiance_hitdist);
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *rRes->pRTVDiff);
}

void DenoiserShading::setupDenoiser()
{
	auto rRes = rendererResources;

	const nrd::MethodDesc methodDescs[] =
	{
		// put neeeded methods here, like:
		{ nrd::Method::REBLUR_DIFFUSE, static_cast<uint16_t>(rRes->winDimensions.x), static_cast<uint16_t>(rRes->winDimensions.y) }
	};

	nrd::DenoiserCreationDesc denoiserCreationDesc = {};
	denoiserCreationDesc.requestedMethods = methodDescs;
	denoiserCreationDesc.requestedMethodNum = static_cast<uint32_t>(std::size(methodDescs));

	destroyDenoiser();
	NRD = make_unique<NrdIntegration>(rRes->numBackBuffers);
	bool res = NRD->Initialize(*nriDevice, *NRI, *NRI, denoiserCreationDesc);
}

void DenoiserShading::destroyDenoiser()
{
	if (NRD)
	{
		NRD->Destroy();
		NRD.reset();
	}
}
