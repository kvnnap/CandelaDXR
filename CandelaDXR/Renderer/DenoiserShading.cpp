#include "DenoiserShading.h"

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

using candela::renderer::DenoiserShading;

namespace candela::renderer
{
	struct NriInterface
		: public nri::CoreInterface
		, public nri::HelperInterface
		, public nri::WrapperD3D12Interface
	{};
}

DenoiserShading::DenoiserShading()
	: rendererResources(), nriDevice()
{
}

DenoiserShading::~DenoiserShading()
{
	NRD->Destroy();
}

void DenoiserShading::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	this->rendererResources = rRes;

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
		{ nrd::Method::REBLUR_DIFFUSE_SPECULAR, rRes->winDimensions.x, rRes->winDimensions.y },
	};

	nrd::DenoiserCreationDesc denoiserCreationDesc = {};
	denoiserCreationDesc.requestedMethods = methodDescs;
	denoiserCreationDesc.requestedMethodNum = 1;

	bool res = NRD->Initialize(*nriDevice, *NRI, *NRI, denoiserCreationDesc);

	// --- END NVIDIA Wrap device
}

void DenoiserShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex)
{
	// Denoising ////////////////////////////////////////////////////

	nri::CommandBufferD3D12Desc cmdDesc = {};
	cmdDesc.d3d12CommandList = pCurrentCommandList.Get();
	cmdDesc.d3d12CommandAllocator = nullptr; // Not needed for NRD integration layer

	nri::CommandBuffer* cmdBuffer = nullptr;
	NRI->CreateCommandBufferD3D12(*nriDevice, cmdDesc, cmdBuffer);

	// Wrap required textures
	constexpr int N = 1;
	nri::TextureTransitionBarrierDesc entryDescs[N] = {};
	nri::Format entryFormat[N] = {};

	for (uint32_t i = 0; i < N; i++)
	{
		nri::TextureTransitionBarrierDesc& entryDesc = entryDescs[i];

		nri::TextureD3D12Desc textureDesc = {};
		textureDesc.d3d12Resource = nullptr; //*pRadAccumulator;
		NRI->CreateTextureD3D12(*nriDevice, textureDesc, (nri::Texture*&)entryDesc.texture);

		// You need to specify the current state of the resource here, after denoising NRD can modify
		// this state. Application must continue state tracking from this point.
		// Useful information:
		//    SRV = nri::AccessBits::SHADER_RESOURCE, nri::TextureLayout::SHADER_RESOURCE
		//    UAV = nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::TextureLayout::GENERAL
		entryDesc.nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
		entryDesc.nextLayout = nri::TextureLayout::GENERAL;
	}

	nrd::CommonSettings commonSettings{};
	auto camera = rendererResources->camera;
	memcpy(&commonSettings.worldToViewMatrix, &camera->getViewMatrix(), sizeof(commonSettings.worldToViewMatrix));
	memcpy(&commonSettings.viewToClipMatrix, &camera->getViewMatrix(), sizeof(commonSettings.viewToClipMatrix));
	nrd::ReblurSettings settings{};

	NRD->SetMethodSettings(nrd::Method::REBLUR_DIFFUSE_SPECULAR, &settings);
	// Fill up the user pool
	NrdUserPool userPool = {};

	{
		//NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_NORMAL_ROUGHNESS)
	}

	NRD->Denoise(currentBackBufferIndex, *cmdBuffer, commonSettings, userPool);

	for (uint32_t i = 0; i < N; i++)
		NRI->DestroyTexture(*(nri::Texture*&)entryDescs[i].texture);

	NRI->DestroyCommandBuffer(*cmdBuffer);
	/////////////////////////////////////////////////////////////////
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
