#include "DenoiserShading.h"

#include <vector>

#include <d3dcompiler.h>
#include "Exception/WindowException.h"
#include "Util/StringUtil.h"
#include "Mathematics/Types.h"
#include "DirectX/DxUtil.h"

#include "NRI.h"
#include "NRIDescs.h"
#include "Extensions/NRIHelper.h"
#include "Extensions/NRIWrapperD3D12.h"
#include "NRDIntegration.h"
#include "NRDIntegration.hpp"

using std::make_unique;
using std::make_shared;
using std::uint32_t;
using std::uint16_t;
using std::vector;

using candela::renderer::DenoiserShading;
using candela::directx::RootSignatureManager;
using candela::directx::Resource;
using candela::directx::DescriptorHeap;
using candela::directx::DXUtil;
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
	: rendererResources(), nriDevice(),
	  diffRadAccumulator(), causticsAccumulator(), diffUnmerged(), specRadAccumulator(), albedo(), 
	  normal(), depth(), gRayHitT(), position(), meshInfo(), matrices(),
	  in_mv(), in_normal_roughness(), in_view_z(),
	  in_diff_radiance_hitdist(), in_spec_radiance_hitdist(),
	  out_diff_radiance_hitdist(), out_spec_radiance_hitdist()
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

	prevMat.clear();
	for (auto sn : rRes->scene->getSceneGraph().getFlattenedMeshNodes())
		prevMat.push_back(sn.ComputedTransform);

	auto mvMats = getMVMatrices();
	matrices = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, static_cast<UINT>(sizeof(decltype(mvMats)::value_type) * mvMats.size()));
	matrices->setName("Denoiser MV Matrices");
	matrices->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	matrices->write(pCurrentCommandList, rendererResources->getTempResource(), mvMats.data());

	// Get resources & create new ones
	auto& dim = rRes->winDimensions;
	diffRadAccumulator = rRes->resourceManager->getNamedResource("diff_acc");
	//causticsAccumulator = rRes->resourceManager->getNamedResource("caustics");
	causticsAccumulator = &rendererResources->resourceManager->createResourceIfNotExists(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "caustics");
	diffUnmerged = &rendererResources->resourceManager->createResourceIfNotExists(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "diff_unmerged");
	specRadAccumulator = rRes->resourceManager->getNamedResource("spec_acc");
	albedo = rRes->resourceManager->getNamedResource("gAlb");
	normal = rRes->resourceManager->getNamedResource("gNorm");
	depth = rRes->resourceManager->getNamedResource("gDepth");
	gRayHitT = rRes->resourceManager->getNamedResource("ray_hitT");
	position = rRes->resourceManager->getNamedResource("gPos");
	meshInfo = rRes->resourceManager->getNamedResource("gMeshInfo");

	in_mv = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "in_mv");
	in_normal_roughness = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "in_normal_roughness");
	in_view_z = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32_FLOAT, true, "in_view_z");
	in_diff_radiance_hitdist = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "in_diff_radiance_hitdist");
	in_spec_radiance_hitdist = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "in_spec_radiance_hitdist");
	out_diff_radiance_hitdist = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "out_diff_radiance_hitdist");
	out_spec_radiance_hitdist = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "out_spec_radiance_hitdist");

	// Setup compute shader RS
	auto rsm = make_shared<RootSignatureManager>();
	CD3DX12_ROOT_PARAMETER1 param;
	rsm->addDescriptorRange("IORange", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10u, 0u));
	rsm->addDescriptorRange("IORange", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 9u, 0u));
	rsm->setDescriptorTableParameter("IODescTable", "IORange");
	param.InitAsConstants(8u, 0u); rsm->setParameter("Constants", param);
	param.InitAsShaderResourceView(10u); rsm->setParameter("Matrices", param);
	param.InitAsShaderResourceView(11u); rsm->setParameter("Materials", param);
	rsm->addParametersToRootSignature("ComputeRootSignature", { "IODescTable", "Constants", "Matrices", "Materials" });
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
	deviceDesc.d3d12GraphicsQueue = rRes->commandQueue->getCommandQueue().Get();
	deviceDesc.enableNRIValidation = false;

	// Wrap the device
	nri::Result result = nri::nriCreateDeviceFromD3D12Device(deviceDesc, nriDevice);

	// Get needed functionality
	result = nri::nriGetInterface(*nriDevice, NRI_INTERFACE(nri::CoreInterface), (nri::CoreInterface*)NRI.get());
	result = nri::nriGetInterface(*nriDevice, NRI_INTERFACE(nri::HelperInterface), (nri::HelperInterface*)NRI.get());

	// Get needed "wrapper" extension, XXX - can be D3D11, D3D12 or VULKAN
	result = nri::nriGetInterface(*nriDevice, NRI_INTERFACE(nri::WrapperD3D12Interface), (nri::WrapperD3D12Interface*)NRI.get());

	NRD = std::vector<std::unique_ptr<NrdIntegration>>(2);
	setupDenoiser();
	// --- END NVIDIA Wrap device
}

void DenoiserShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex)
{
	wrl::ComPtr<ID3D12DebugCommandList> pDbgCmdList;
	pCurrentCommandList.As(&pDbgCmdList);

	// Update Mat
	matrices->write(pCurrentCommandList, rendererResources->getTempResource(), getMVMatrices().data());

	// Compute pre-pass
	diffRadAccumulator->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	specRadAccumulator->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	albedo->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	normal->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	position->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	meshInfo->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	depth->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	gRayHitT->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	rendererResources->pRTVDiff->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	rendererResources->pRTVSpec->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	compute(pCurrentCommandList, 0);

	in_mv->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	in_normal_roughness->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	in_view_z->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	in_diff_radiance_hitdist->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	in_spec_radiance_hitdist->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

	/// Pass to convert radiance without albedo - compute shader required


	// Denoising ////////////////////////////////////////////////////

	nri::CommandBufferD3D12Desc cmdDesc = {};
	cmdDesc.d3d12CommandList = pCurrentCommandList.Get();
	cmdDesc.d3d12CommandAllocator = nullptr; // rendererResources->commandQueue->getCommandAllocator(pCurrentCommandList); // Not needed for NRD integration layer

	nri::CommandBuffer* cmdBuffer = nullptr;
	NRI->CreateCommandBufferD3D12(*nriDevice, cmdDesc, cmdBuffer);

	// Wrap required textures
	constexpr int N = 7;
	nri::TextureTransitionBarrierDesc entryDescs[N] = {};
	nri::Format entryFormat[N] = {};
	DenoiserResource denoiserResources[N] = {
		{ in_mv,                     D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nrd::ResourceType::IN_MV,                     nri::Format::RGBA32_SFLOAT },
		{ in_normal_roughness,       D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nrd::ResourceType::IN_NORMAL_ROUGHNESS,       nri::Format::RGBA32_SFLOAT },
		{ in_view_z,                 D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nrd::ResourceType::IN_VIEWZ,                  nri::Format::R32_SFLOAT    },
		{ in_diff_radiance_hitdist,  D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST,  nri::Format::RGBA32_SFLOAT },
		{ in_spec_radiance_hitdist,  D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST,  nri::Format::RGBA32_SFLOAT },
		{ out_diff_radiance_hitdist, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,    nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, nri::Format::RGBA32_SFLOAT },
		{ out_spec_radiance_hitdist, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,    nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST, nri::Format::RGBA32_SFLOAT }
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
	auto viewMatrix = camera->getViewMatrix();
	auto persMatrix = camera->getPerspectiveMatrix();

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
	//nrdReblurSettings.hitDistanceParameters;
	//nrdReblurSettings.hitDistanceReconstructionMode; --- NEED FOR LightTracing!
	
	NRD[denoiserSelected]->NewFrame();
	NRD[denoiserSelected]->SetCommonSettings(nrdCommonSettings);
	if (denoiserSelected == 0)
		NRD[denoiserSelected]->SetDenoiserSettings(denoiserSelected, &nrdReblurSettings);
	else 
		NRD[denoiserSelected]->SetDenoiserSettings(denoiserSelected, &nrdRelaxSettings);
	
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
	
	const nrd::Identifier denoisers[] = { denoiserSelected };
	NRD[denoiserSelected]->Denoise(denoisers, static_cast<uint32_t>(std::size(denoisers)), *cmdBuffer, userPool);
	
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
	in_spec_radiance_hitdist->transitionToPrevBarrier(pCurrentCommandList);

	compute(pCurrentCommandList, 1);

	diffRadAccumulator->transitionToPrevBarrier(pCurrentCommandList);
	specRadAccumulator->transitionToPrevBarrier(pCurrentCommandList);
	albedo->transitionToPrevBarrier(pCurrentCommandList);
	normal->transitionToPrevBarrier(pCurrentCommandList);
	position->transitionToPrevBarrier(pCurrentCommandList);
	meshInfo->transitionToPrevBarrier(pCurrentCommandList);
	depth->transitionToPrevBarrier(pCurrentCommandList);
	gRayHitT->transitionToPrevBarrier(pCurrentCommandList);
	rendererResources->pRTVDiff->transitionToPrevBarrier(pCurrentCommandList);
	rendererResources->pRTVSpec->transitionToPrevBarrier(pCurrentCommandList);
	causticsAccumulator->uavBarrier(pCurrentCommandList);
	diffUnmerged->uavBarrier(pCurrentCommandList);

	nrdCommonSettings.accumulationMode = nrd::AccumulationMode::CONTINUE;
}

void DenoiserShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::Statistics))
		clearHistory();
}

void DenoiserShading::onResize()
{
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

std::uint32_t DenoiserShading::getBufferUsage() const
{
	return BufferUsage::Diffuse | BufferUsage::Specular;
}

nrd::CommonSettings& DenoiserShading::getCommonSettings()
{
	return nrdCommonSettings;
}

nrd::ReblurSettings& DenoiserShading::getReblurSettings()
{
	return nrdReblurSettings;
}

nrd::RelaxDiffuseSpecularSettings& DenoiserShading::getRelaxSettings()
{
	return nrdRelaxSettings;
}

void DenoiserShading::clearHistory()
{
	nrdCommonSettings.accumulationMode = nrd::AccumulationMode::CLEAR_AND_RESTART;
}

vector<DirectX::XMFLOAT3X4> DenoiserShading::getMVMatrices()
{
	vector<DirectX::XMFLOAT3X4> mats(prevMat.size());
	const auto sMats = rendererResources->scene->getSceneGraph().getFlattenedMeshNodes();
	for (std::size_t i = 0; i < prevMat.size(); ++i)
	{
		auto copyOfCurrentMat = sMats.at(i).ComputedTransform;
		auto mat = copyOfCurrentMat; // Get current local to World transform
		mat = DirectX::XMMatrixInverse(nullptr, mat); // Invert It - becomes world to local
		mat *= prevMat[i]; // Then apply previous local to world transform
		DirectX::XMStoreFloat3x4(&mats[i], mat);
		prevMat[i] = copyOfCurrentMat;
	}
	return mats;
}

void DenoiserShading::setDenoiserSelected(std::uint32_t den)
{
	if (den >= 2) return;
	denoiserSelected = den;
	clearHistory();
}

uint32_t DenoiserShading::getDenoiseCaustics() const
{
	return denoiseCaustics;
}

void DenoiserShading::setDenoiseCaustics(std::uint32_t den)
{
	if (den >= 2) return;
	denoiseCaustics = den;
	clearHistory();
}

uint32_t DenoiserShading::getDenoiserSelected() const
{
	return denoiserSelected;
}

void DenoiserShading::compute(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t mode)
{
	out_diff_radiance_hitdist->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	out_spec_radiance_hitdist->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	pCurrentCommandList->SetComputeRootSignature(computeRootSignature.Get());
	pCurrentCommandList->SetPipelineState(computePipelineState.Get());
	pCurrentCommandList->SetDescriptorHeaps(1u, descHeapManager->getDescriptorHeap().GetAddressOf());
	pCurrentCommandList->SetComputeRootDescriptorTable(0u, descHeapManager->getDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	pCurrentCommandList->SetComputeRoot32BitConstants(1u, 4u, &nrdReblurSettings.hitDistanceParameters, 0u);
	pCurrentCommandList->SetComputeRoot32BitConstants(1u, 1u, &rendererResources->camera->getPosition().m128_f32[2], 4u);
	pCurrentCommandList->SetComputeRoot32BitConstant(1u, mode, 5u);
	pCurrentCommandList->SetComputeRoot32BitConstant(1u, denoiserSelected, 6u);
	pCurrentCommandList->SetComputeRoot32BitConstant(1u, denoiseCaustics, 7u);

	pCurrentCommandList->SetComputeRootShaderResourceView(2u, ((ID3D12Resource*)*matrices)->GetGPUVirtualAddress());
	pCurrentCommandList->SetComputeRootShaderResourceView(3u, rendererResources->materialBuffer->GetGPUVirtualAddress());
	constexpr auto ThreadGroupDim = 8u;
	auto& dim = rendererResources->winDimensions;
	auto launchDimensions = UVector2(dim.x / ThreadGroupDim + (dim.x % ThreadGroupDim == 0u ? 0u : 1u), dim.y / ThreadGroupDim + (dim.y % ThreadGroupDim == 0u ? 0u : 1u));
	pCurrentCommandList->Dispatch(launchDimensions.x, launchDimensions.y, 1u);

	out_diff_radiance_hitdist->transitionToPrevBarrier(pCurrentCommandList);
	out_spec_radiance_hitdist->transitionToPrevBarrier(pCurrentCommandList);
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
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *diffRadAccumulator);
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *specRadAccumulator);
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *albedo);
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *normal);
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *depth);
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *gRayHitT);
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *out_diff_radiance_hitdist);
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *out_spec_radiance_hitdist);
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *position);
	srvDesc.Format = DXGI_FORMAT_R32G32_UINT;
	descHeapManager->setSRV(entryNum++, srvDesc, rRes->pDevice, *meshInfo);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_mv);
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_normal_roughness);

	uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_view_z);

	uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_diff_radiance_hitdist);
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *in_spec_radiance_hitdist);
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *rRes->pRTVDiff);
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *rRes->pRTVSpec);
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *causticsAccumulator);
	descHeapManager->setUAV(entryNum++, uavDesc, rRes->pDevice, *diffUnmerged);
}

void DenoiserShading::setupDenoiser()
{
	auto rRes = rendererResources;
	destroyDenoiser();

	for (uint32_t i = 0; i < NRD.size(); ++i)
	{

		const nrd::DenoiserDesc denoiserDescs[] =
		{
			// put neeeded methods here, like:
			{ i, i == 0 ? nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR : nrd::Denoiser::RELAX_DIFFUSE_SPECULAR, static_cast<uint16_t>(rRes->winDimensions.x), static_cast<uint16_t>(rRes->winDimensions.y) }
		};

		nrd::InstanceCreationDesc instanceCreationDesc = {};
		instanceCreationDesc.denoisers = denoiserDescs;
		instanceCreationDesc.denoisersNum = static_cast<uint32_t>(std::size(denoiserDescs));

		NRD[i] = make_unique<NrdIntegration>(rRes->numBackBuffers, false);
		bool res = NRD[i]->Initialize(instanceCreationDesc, *nriDevice, *NRI, *NRI);
	}
}

void DenoiserShading::destroyDenoiser()
{
	for (auto& nrd : NRD)
	{
		if (nrd)
		{
			nrd->Destroy();
			nrd.reset();
		}
	}
}
