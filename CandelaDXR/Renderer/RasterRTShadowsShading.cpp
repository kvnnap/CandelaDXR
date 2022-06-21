#include <cstdint>

#include <d3dcompiler.h>

#include "DirectX/DxUtil.h"
#include "Exception/Exception.h"
#include "Exception/WindowException.h"

#include "AccelerationStructure.h"
#include "RendererResources.h"

#include "Util/StringUtil.h"

#include "RasterRTShadowsShading.h"

using std::int32_t;
using std::uint32_t;
using std::unique_ptr;
using std::make_unique;
using std::make_shared;

using Microsoft::WRL::ComPtr;

using candela::directx::DXUtil;
using candela::directx::RootSignatureManager;
using candela::directx::DescriptorHeap;
using candela::directx::ShadingTable;
using candela::directx::ShadingRecordType;
using candela::directx::Resource;

using candela::sampler::ISampler;

using candela::util::StringToWString;

using candela::renderer::AccelerationStructure;
using candela::renderer::RendererResources;
using candela::renderer::ResourceRegFunction;
using candela::renderer::RasterRTShadowsShading;

RasterRTShadowsShading::RasterRTShadowsShading(unique_ptr<ISampler> sampler)
	: rasterShader(true), rendererResources(), constBuffer(), sampler(std::move(sampler)), clear()
{
}

void RasterRTShadowsShading::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	rasterShader.setGlobaResourcePrefix("rrt_");
	rasterShader.init(rRes, pCurrentCommandList, resRegFn);

	if (!DXUtil::checkDeviceRTSupport(rRes->pDevice))
		ThrowException("Ray tracing is not supported on this device");

	if (!rRes->accelerationStructure)
	{
		auto accel = make_unique<AccelerationStructure>();
		rRes->accelerationStructure = accel.get();
		resRegFn(std::move(accel));
	}

	// Take reference of the rendering resources
	rendererResources = rRes;

	// Const buffer initial values
	constBuffer.numLights = static_cast<uint32_t>(rRes->scene->getLights().size());

	// Build Pipeline
	buildPipeline();

	// The output resource
	radianceTexture = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		rRes->winDimensions.x, rRes->winDimensions.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "rt_shad_rad");
	radianceTexture->setName(L"RasterRT Radiance Texture");

	// Create Shader resources
	createShaderResources();

	// Constant buffer
	constantBuffer = DXUtil::uploadDataToDefaultHeap(rRes->pDevice, pCurrentCommandList, rendererResources->getTempResource(), &constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	constantBuffer->SetName(L"Raster ShadowsRT Constant Buffer");

	// Build shading table
	createShaderTable(pCurrentCommandList);
}

void RasterRTShadowsShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex)
{
	rasterShader.draw(pCurrentCommandList, currentBackBufferIndex);

	// Pre-stuff
	auto& backBuff = rendererResources->pRTVRadBackBuffer;
	backBuff->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// Do we need these barriers? I think so
	const auto c = 3;
	for (UINT i = 0; i < c; ++i)
		rasterShader.getGBuffer()[i]->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Copy and update camera
	auto cam = rendererResources->camera;
	constBuffer.seeds[0] = sampler->nextUInt32();
	constBuffer.seeds[1] = sampler->nextUInt32();
	constBuffer.winDimensions = rendererResources->winDimensions;
	if (clear)
		constBuffer.frameNumber = 0;
	++constBuffer.frameNumber;
	DXUtil::updateDataInDefaultHeap(rendererResources->pDevice, pCurrentCommandList, constantBuffer, 
		rendererResources->getTempResource(),
		&constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	pCurrentCommandList->SetDescriptorHeaps(1u, descHeapManager->getDescriptorHeap().GetAddressOf());
	pCurrentCommandList->SetComputeRootSignature(globalEmptyRootSignature.Get());
	HRESULT hr;
	ComPtr<ID3D12GraphicsCommandList4> commandList4;
	GFXTHROWIFFAILED(pCurrentCommandList.As(&commandList4));
	commandList4->SetPipelineState1(stateObject.Get());

	// Launch rays
	auto rayDimensions = rendererResources->winDimensions;
	D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = shadingTable->getDispatchRaysDescriptor(rayDimensions.x, rayDimensions.y);
	commandList4->DispatchRays(&dispatchRaysDesc);
	clear = false;

	// After
	for (UINT i = 0; i < c; ++i)
		rasterShader.getGBuffer()[i]->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	backBuff->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void RasterRTShadowsShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	rasterShader.onChange(pCurrentCommandList, currentBackBufferIndex, changeEvent);

	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::SceneChange))
	{
		constBuffer.numLights = static_cast<uint32_t>(rendererResources->scene->getLights().size());
		createShaderTable(pCurrentCommandList);
	}
	clear = true;
}

void RasterRTShadowsShading::onResize()
{
	rasterShader.onResize();
	createShaderResources();
	clear = true;
}

void RasterRTShadowsShading::accept(IVisitor* visitor)
{
	visitor->visit(this);
}

uint32_t RasterRTShadowsShading::getLightType() const
{
	return constBuffer.lightType;
}

void RasterRTShadowsShading::setLightType(uint32_t lightType)
{
	constBuffer.lightType = lightType;
	rasterShader.setComputeRadiance(lightType == 0 ? 1 : 0);
}

void RasterRTShadowsShading::buildPipeline()
{
	rootSignatureManager = make_shared<RootSignatureManager>();

	HRESULT hr;

	// Define State Object Descriptor (EXTENDED version from d3dx)
	CD3DX12_STATE_OBJECT_DESC stateObjectDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	// Construct sub objects

	// First is DXIL - to load shader and Load symbols from the shader and identify the entry points
	CD3DX12_DXIL_LIBRARY_SUBOBJECT dxilSubObject(stateObjectDesc);
	const WCHAR* entryPoints[] = { L"rayGen", L"shadowMiss" };
	dxilSubObject.DefineExports(entryPoints);

	// Second - Hit Program - link to entry point names
	CD3DX12_HIT_GROUP_SUBOBJECT hitSubObject(stateObjectDesc);
	hitSubObject.SetHitGroupExport(L"HitGroup");

	// Third - Local Root Signature for Ray Gen shader
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0)); //gOutput, gRadiance
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 10)); //gRtScene, gPos, gNorm, gAlb
	if (!rendererResources->textures.empty())
		rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(rendererResources->textures.size()), 14));

	rootSignatureManager->setDescriptorTableParameter("BVHDescTable", "BVH");
	CD3DX12_ROOT_PARAMETER1 param;
	param.InitAsConstantBufferView(0);
	rootSignatureManager->setParameter("ConstBuff", param);

	param.InitAsShaderResourceView(0); rootSignatureManager->setParameter("verts", param);
	param.InitAsShaderResourceView(1); rootSignatureManager->setParameter("texVerts", param);
	param.InitAsShaderResourceView(2); rootSignatureManager->setParameter("normals", param);
	param.InitAsShaderResourceView(3); rootSignatureManager->setParameter("indices", param);
	param.InitAsShaderResourceView(4); rootSignatureManager->setParameter("matrices", param);
	param.InitAsShaderResourceView(5); rootSignatureManager->setParameter("normalMatrices", param);
	param.InitAsShaderResourceView(6); rootSignatureManager->setParameter("faceAttributes", param);
	param.InitAsShaderResourceView(7); rootSignatureManager->setParameter("materials", param);
	param.InitAsShaderResourceView(8); rootSignatureManager->setParameter("lights", param);

	rootSignatureManager->addParametersToRootSignature("RayGenRootSignature", { "BVHDescTable", "ConstBuff", "verts", "texVerts", "normals", "indices", "matrices", "normalMatrices", "faceAttributes", "materials", "lights" });
	rootSignatureManager->setSamplerForRootSignature("RayGenRootSignature", DXUtil::getDefaultSamplerDesc());
	rootSignatureManager->generateRootSignature("RayGenRootSignature", rendererResources->pDevice);

	rootSignatureManager->addRootSignature("EmptyRootSignature");
	rootSignatureManager->generateRootSignature("EmptyRootSignature", rendererResources->pDevice);

	// Generate/add subobjects
	rootSignatureManager->addRootSignaturesToSubObject(stateObjectDesc);

	// Seventh - Shader Configuration (set payload sizes - the actual program parameters)
	CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT shaderConfig(stateObjectDesc);
	shaderConfig.Config(4 * sizeof(float), 2 * sizeof(float));

	// Eighth - Associate the shader configuration with all shader programs
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT shaderConfigAssociation(stateObjectDesc);
	LPCWSTR exports[] = { L"rayGen", L"shadowMiss" };
	shaderConfigAssociation.AddExports(exports);
	shaderConfigAssociation.SetSubobjectToAssociate(shaderConfig);

	// Ninth - Configure the RAY TRACING PIPELINE
	CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT rtPipelineConfig(stateObjectDesc);
	rtPipelineConfig.Config(1);

	// Tenth - Global Root Signature
	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT globalRootSignature(stateObjectDesc);
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC globalEmptyRootSignatureDesc(0, static_cast<CD3DX12_ROOT_PARAMETER1*>(nullptr), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
	globalEmptyRootSignature = DXUtil::createRootSignature(rendererResources->pDevice, globalEmptyRootSignatureDesc);
	globalEmptyRootSignature->SetName(L"Root Signature Global");
	globalRootSignature.SetRootSignature(globalEmptyRootSignature.Get());

	// Finally - Create the state
	ComPtr<ID3D12Device5> pDevice5;
	GFXTHROWIFFAILED(rendererResources->pDevice.As(&pDevice5));
	
	shadingTable = make_unique<ShadingTable>(rootSignatureManager);
	shadingTable->addProgram(L"rayGen", ShadingRecordType::RayGeneration, "RayGenRootSignature");
	shadingTable->addProgram(L"shadowMiss", ShadingRecordType::Miss, "EmptyRootSignature");
	shadingTable->addProgramAssociationsToSubobject(stateObjectDesc);
	wrl::ComPtr<ID3DBlob> pBlob;
	GFXTHROWIFFAILED(D3DReadFileToBlob(StringToWString("./Shaders/RTShadows.cso").c_str(), &pBlob));
	auto shaderByteCodeDesc = CD3DX12_SHADER_BYTECODE(pBlob.Get());
	dxilSubObject.SetDXILLibrary(&shaderByteCodeDesc);
	GFXTHROWIFFAILED(pDevice5->CreateStateObject(stateObjectDesc, IID_PPV_ARGS(&stateObject)));
}

void RasterRTShadowsShading::createShaderResources()
{
	const auto& dim = rendererResources->winDimensions;

	// Generate paramter instance i - switch between these later
	size_t entryNumber = 0;
	if (!descHeapManager)
	{
		descHeapManager = make_shared<DescriptorHeap>(rootSignatureManager, "BVHDescTable", "BVH1", rendererResources->pDevice);
		shadingTable->setDescriptorHeapIfNotExists(descHeapManager);
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	descHeapManager->setUAV(entryNumber++, uavDesc, rendererResources->pDevice, *rendererResources->pRTVRadBackBuffer);
	descHeapManager->setUAV(entryNumber++, uavDesc, rendererResources->pDevice, *radianceTexture);

	// Create the SRV descriptor in second place (following same order as in root signature)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = rendererResources->accelerationStructure->getTopLayerBufferAddress();
	descHeapManager->setSRV(entryNumber++, srvDesc, rendererResources->pDevice);

	// gBuffer
	srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	const auto gBuffRenTargets = 3;
	for (UINT j = 0; j < gBuffRenTargets; ++j)
		descHeapManager->setSRV(entryNumber++, srvDesc, rendererResources->pDevice, *rasterShader.getGBuffer()[j]);

	// Textures
	srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	for (const auto& texture : rendererResources->textures)
		descHeapManager->setSRV(entryNumber++, srvDesc, rendererResources->pDevice, texture);
}

void RasterRTShadowsShading::createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList)
{
	// Link rayGen
	shadingTable->setInputForDescriptorTableParameter(L"rayGen", "BVHDescTable", "BVH1");

	// TODO - setInputForViewParameter should also accept and array to apply a resource to more than one view parameter
	shadingTable->setInputForViewParameter(L"rayGen", "ConstBuff", constantBuffer);
	shadingTable->setInputForViewParameter(L"rayGen", "verts", rendererResources->sceneBuffer, rendererResources->scene->getVerticesOffset());
	shadingTable->setInputForViewParameter(L"rayGen", "texVerts", rendererResources->sceneBuffer, rendererResources->scene->getTextureCoordsOffset());
	shadingTable->setInputForViewParameter(L"rayGen", "normals", rendererResources->sceneBuffer, rendererResources->scene->getNormalsOffset());
	shadingTable->setInputForViewParameter(L"rayGen", "indices", rendererResources->sceneBuffer, rendererResources->scene->getIndicesOffset());
	shadingTable->setInputForViewParameter(L"rayGen", "matrices", rendererResources->matrices);
	shadingTable->setInputForViewParameter(L"rayGen", "normalMatrices", rendererResources->normalMatrices);
	shadingTable->setInputForViewParameter(L"rayGen", "faceAttributes", rendererResources->faceAttributeBuffer);
	shadingTable->setInputForViewParameter(L"rayGen", "materials", rendererResources->materialBuffer);
	shadingTable->setInputForViewParameter(L"rayGen", "lights", rendererResources->lightBuffer);

	// Generate
	shadingTable->generateShadingTable(rendererResources->pDevice, commandList, stateObject, rendererResources->getTempResource());
}
