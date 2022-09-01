#include "DenoiserShading.h"

#include <d3dcompiler.h>
#include "Exception/WindowException.h"
#include "Util/StringUtil.h"
#include "Mathematics/Types.h"

#include "NRD.h"
#include "NRIDescs.h"
#include "NRI.h"
#include "NRIDescs.hpp"
#include "Extensions/NRIHelper.h"
#include "NVIDIA/NRDIntegration.h"
#include "NVIDIA/NRDIntegration.hpp"
#include "Extensions/NRIWrapperD3D12.h"
#include "Extensions/NRIHelper.h"

using std::make_unique;
using std::make_shared;

using candela::renderer::DenoiserShading;
using candela::directx::RootSignatureManager;
using candela::directx::DescriptorHeap;
using candela::mathematics::UVector2;

namespace candela::renderer
{
	struct NriInterface
		: public nri::CoreInterface
		, public nri::HelperInterface
		, public nri::WrapperD3D12Interface
	{};
}

DenoiserShading::DenoiserShading()
	: rendererResources(), nriDevice(), rasterShader(true)
{
}

DenoiserShading::~DenoiserShading()
{
	if (NRD)
		NRD->Destroy();
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
	radAccumulator = rRes->resourceManager->getNamedResource("rad_accumulator");
	albedo = rRes->resourceManager->getNamedResource("den_gAlb");
	normal = rRes->resourceManager->getNamedResource("den_gNorm");
	depth = rRes->resourceManager->getNamedResource("den_gDepth");
	
	in_mv = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "in_mv");
	in_normal_roughness = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "in_normal_roughness");
	in_view_z = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32_FLOAT, true, "in_view_z");
	in_diff_radiance_hitdist = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "in_diff_radiance_hitdist");
	out_diff_radiance_hitdist = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "out_diff_radiance_hitdist");

	// Setup compute shader RS
	auto rsm = make_shared<RootSignatureManager>();
	CD3DX12_ROOT_PARAMETER1 param;
	rsm->addDescriptorRange("IORange", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4u, 0u));
	rsm->addDescriptorRange("IORange", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5u, 0u));
	rsm->setDescriptorTableParameter("IODescTable", "IORange");
	param.InitAsConstants(3u, 0u); rsm->setParameter("Constants", param);
	rsm->addParametersToRootSignature("ComputeRootSignature", { "IODescTable", "Constants" });
	computeRootSignature = rsm->generateRootSignature("ComputeRootSignature", rRes->pDevice, D3D12_ROOT_SIGNATURE_FLAG_NONE);

	// Heap
	descHeapManager = make_unique<DescriptorHeap>(rsm, "IODescTable", "IO1", rRes->pDevice);

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

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_mv);
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_normal_roughness);

	uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_view_z);
	
	uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_diff_radiance_hitdist);
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *out_diff_radiance_hitdist);

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
	NRD = make_unique<NrdIntegration>(rendererResources->numBackBuffers);
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

	const nrd::MethodDesc methodDescs[] =
	{
		// put neeeded methods here, like:
		{ nrd::Method::REBLUR_DIFFUSE, rRes->winDimensions.x, rRes->winDimensions.y },
	};

	nrd::DenoiserCreationDesc denoiserCreationDesc = {};
	denoiserCreationDesc.requestedMethods = methodDescs;
	denoiserCreationDesc.requestedMethodNum = 1;

	bool res = NRD->Initialize(*nriDevice, *NRI, *NRI, denoiserCreationDesc);
	// --- END NVIDIA Wrap device
}

void DenoiserShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex)
{
	static int x = 0;
	++x;

	//wrl::ComPtr<ID3D12GraphicsCommandList> graph;
	//pCurrentCommandList.As(&graph);
	//auto myptr = graph.Get();

	wrl::ComPtr<ID3D12DebugCommandList> pDbgCmdList;
	pCurrentCommandList.As(&pDbgCmdList);
	//pDbgCmdList->SetFeatureMask(D3D12_DEBUG_FEATURE_ALLOW_BEHAVIOR_CHANGING_DEBUG_AIDS);
	//bool isUAV = pDbgCmdList->AssertResourceState(*out_diff_radiance_hitdist, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	rasterShader.draw(pCurrentCommandList, currentBackBufferIndex);

	// Compute pre-pass
	radAccumulator->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	albedo->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	normal->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	depth->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	compute(pCurrentCommandList, 0);

	in_mv->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	in_normal_roughness->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	in_view_z->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	in_diff_radiance_hitdist->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	//static int test = 0;
	//if (test++ > 0)
	//{
	//	radAccumulator->transitionToPrevBarrier(pCurrentCommandList);
	//	albedo->transitionToPrevBarrier(pCurrentCommandList);
	//	normal->transitionToPrevBarrier(pCurrentCommandList);
	//	depth->transitionToPrevBarrier(pCurrentCommandList);
	//	in_mv->transitionToPrevBarrier(pCurrentCommandList);
	//	in_normal_roughness->transitionToPrevBarrier(pCurrentCommandList);
	//	in_view_z->transitionToPrevBarrier(pCurrentCommandList);
	//	in_diff_radiance_hitdist->transitionToPrevBarrier(pCurrentCommandList);
	//	return;
	//}

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

	//for (uint32_t i = 0; i < N; i++)
	{
		// You need to specify the current state of the resource here, after denoising NRD can modify
		// this state. Application must continue state tracking from this point.
		// Useful information:
		//    SRV = nri::AccessBits::SHADER_RESOURCE, nri::TextureLayout::SHADER_RESOURCE
		//    UAV = nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::TextureLayout::GENERAL
		nri::TextureD3D12Desc textureDesc = {};

		textureDesc.d3d12Resource = *in_mv;
		NRI->CreateTextureD3D12(*nriDevice, textureDesc, (nri::Texture*&)entryDescs[0].texture);
		entryDescs[0].nextAccess = nri::AccessBits::SHADER_RESOURCE;
		entryDescs[0].nextLayout = nri::TextureLayout::SHADER_RESOURCE;

		textureDesc.d3d12Resource = *in_normal_roughness;
		NRI->CreateTextureD3D12(*nriDevice, textureDesc, (nri::Texture*&)entryDescs[1].texture);
		entryDescs[1].nextAccess = nri::AccessBits::SHADER_RESOURCE;
		entryDescs[1].nextLayout = nri::TextureLayout::SHADER_RESOURCE;

		textureDesc.d3d12Resource = *in_view_z;
		NRI->CreateTextureD3D12(*nriDevice, textureDesc, (nri::Texture*&)entryDescs[2].texture);
		entryDescs[2].nextAccess = nri::AccessBits::SHADER_RESOURCE;
		entryDescs[2].nextLayout = nri::TextureLayout::SHADER_RESOURCE;

		textureDesc.d3d12Resource = *in_diff_radiance_hitdist;
		NRI->CreateTextureD3D12(*nriDevice, textureDesc, (nri::Texture*&)entryDescs[3].texture);
		entryDescs[3].nextAccess = nri::AccessBits::SHADER_RESOURCE;
		entryDescs[3].nextLayout = nri::TextureLayout::SHADER_RESOURCE;

		textureDesc.d3d12Resource = *out_diff_radiance_hitdist;
		NRI->CreateTextureD3D12(*nriDevice, textureDesc, (nri::Texture*&)entryDescs[4].texture);
		entryDescs[4].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
		entryDescs[4].nextLayout = nri::TextureLayout::GENERAL;
	}

	nrd::CommonSettings commonSettings{};
	auto camera = rendererResources->camera;
	memcpy(&commonSettings.worldToViewMatrix, &camera->getViewMatrix(), sizeof(commonSettings.worldToViewMatrix));
	memcpy(&commonSettings.worldToViewMatrixPrev, &camera->getViewMatrix(), sizeof(commonSettings.worldToViewMatrixPrev));
	memcpy(&commonSettings.viewToClipMatrix, &camera->getPerspectiveMatrix(), sizeof(commonSettings.viewToClipMatrix));
	memcpy(&commonSettings.viewToClipMatrixPrev, &camera->getPerspectiveMatrix(), sizeof(commonSettings.viewToClipMatrixPrev));
	nrd::ReblurSettings settings{};
	settings.enableReferenceAccumulation = true;
	nrd::RelaxDiffuseSettings settings2{};

	NRD->SetMethodSettings(nrd::Method::REBLUR_DIFFUSE, &settings);
	//NRD->SetMethodSettings(nrd::Method::RELAX_DIFFUSE, &settings2);
	// Fill up the user pool
	NrdUserPool userPool = {};

	{
		NrdIntegrationTexture tex{};
		tex.format = nri::Format::RGBA32_SFLOAT;
		tex.subresourceStates = &entryDescs[0];
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_MV, tex);

		tex.subresourceStates = &entryDescs[1];
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_NORMAL_ROUGHNESS, tex);

		tex.subresourceStates = &entryDescs[2];
		tex.format = nri::Format::R32_SFLOAT;
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_VIEWZ, tex);

		tex.subresourceStates = &entryDescs[3];
		tex.format = nri::Format::RGBA32_SFLOAT;
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST, tex);

		tex.subresourceStates = &entryDescs[4];
		NrdIntegration_SetResource(userPool, nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, tex);
	}
	
	bool isUAV = pDbgCmdList->AssertResourceState(*out_diff_radiance_hitdist, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	NRD->Denoise(currentBackBufferIndex, *cmdBuffer, commonSettings, userPool);
	isUAV = pDbgCmdList->AssertResourceState(*out_diff_radiance_hitdist, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	// Sync states
	if (entryDescs[4].nextAccess != nri::AccessBits::SHADER_RESOURCE_STORAGE)
	{
		out_diff_radiance_hitdist->rewriteState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		out_diff_radiance_hitdist->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	//bool isUAV = pDbgCmdList->AssertResourceState(*out_diff_radiance_hitdist, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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
}

void DenoiserShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
}

void DenoiserShading::onResize()
{
}

void DenoiserShading::accept(IVisitor* visitor)
{
}

void DenoiserShading::compute(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t mode)
{
	if (mode == 1)
		out_diff_radiance_hitdist->uavBarrier(pCurrentCommandList);

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

	if (mode == 1)
		out_diff_radiance_hitdist->uavBarrier(pCurrentCommandList);
}
