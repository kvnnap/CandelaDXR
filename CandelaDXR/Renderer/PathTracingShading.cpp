#include "PathTracingShading.h"

#include <cstdint>

#include <d3dcompiler.h>

#include "DirectX/DxUtil.h"
#include "Exception/Exception.h"
#include "Exception/WindowException.h"

#include "AccelerationStructure.h"
#include "RendererResources.h"

#include "Util/StringUtil.h"

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
using candela::renderer::PathTracingShading;

PathTracingShading::PathTracingShading(unique_ptr<ISampler> sampler, bool specularOnly)
	: rendererResources(), constBuffer(), sampler(std::move(sampler)), clear()
{
	setSpecularOnly(specularOnly);
}

void PathTracingShading::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
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
	constBuffer.pathFilter = 0xFFFFFFFF;

	// Build Pipeline
	buildPipeline();

	// Create Shader resources
	createShaderResources();

	// Constant buffer
	constantBuffer = DXUtil::uploadDataToDefaultHeap(rRes->pDevice, pCurrentCommandList, rendererResources->getTempResource(), &constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	constantBuffer->SetName(L"PT Constant Buffer");

	// Build shading table
	createShaderTable(pCurrentCommandList, rendererResources->getTempResource());
}

void PathTracingShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex)
{
	// Pre-stuff
	auto& backBuff = rendererResources->pRTVRadBackBuffer;
	backBuff->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// Copy and update camera
	auto cam = rendererResources->camera;
	constBuffer.w = DirectX::XMVector3Normalize(cam->getDirection());
	constBuffer.u = DirectX::XMVectorNegate(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cam->getUp(), cam->getDirection())));
	constBuffer.v = DirectX::XMVectorNegate(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(constBuffer.w, constBuffer.u)));
	constBuffer.position = cam->getPosition();
	constBuffer.direction = cam->getDirection();
	constBuffer.plane = cam->getNearPlaneDimensions();
	constBuffer.seeds[0] = sampler->nextUInt32();
	constBuffer.seeds[1] = sampler->nextUInt32();
	constBuffer.winDimensions = rendererResources->winDimensions;
	clear |= cam->hasChanged();
	if (clear)
		constBuffer.frameNumber = 1;
	else
		++constBuffer.frameNumber;
	DXUtil::updateDataInDefaultHeap(rendererResources->pDevice, pCurrentCommandList, constantBuffer, rendererResources->getTempResource(),
		&constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	pCurrentCommandList->SetDescriptorHeaps(1u, descriptorHeap.GetAddressOf());
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
	backBuff->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void PathTracingShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::SceneChange))
	{
		constBuffer.numLights = static_cast<uint32_t>(rendererResources->scene->getLights().size());
		createShaderTable(pCurrentCommandList, rendererResources->getTempResource());
	}
	clear = true;
}

void PathTracingShading::onResize()
{
	createShaderResources();
	clear = true;
}

void PathTracingShading::accept(IVisitor* visitor)
{
	visitor->visit(this);
}

void PathTracingShading::setSpecularOnly(bool specularOnly)
{
	constBuffer.specularOnly = specularOnly;
}

bool PathTracingShading::getSpecularOnly() const
{
	return constBuffer.specularOnly;
}

void PathTracingShading::setPathFilter(std::uint32_t pathFilter)
{
	constBuffer.pathFilter = pathFilter;
}

uint32_t PathTracingShading::getPathFilter() const
{
	return constBuffer.pathFilter;
}

uint32_t PathTracingShading::getMinBounces() const
{
	return constBuffer.minBounces;
}

void PathTracingShading::setMinBounces(uint32_t minBounces)
{
	constBuffer.minBounces = minBounces;
}

uint32_t PathTracingShading::getMaxBounces() const
{
	return constBuffer.maxBounces;
}

void PathTracingShading::setMaxBounces(uint32_t maxBounces)
{
	constBuffer.maxBounces = maxBounces;
}

void PathTracingShading::buildPipeline()
{
	rootSignatureManager = make_shared<RootSignatureManager>();

	HRESULT hr;

	// Define State Object Descriptor (EXTENDED version from d3dx)
	CD3DX12_STATE_OBJECT_DESC stateObjectDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	// Construct sub objects

	// First is DXIL - to load shader and Load symbols from the shader and identify the entry points
	CD3DX12_DXIL_LIBRARY_SUBOBJECT dxilSubObject(stateObjectDesc);
	const WCHAR* entryPoints[] = { L"rayGen", L"miss", L"chs", L"shadowMiss" };
	dxilSubObject.DefineExports(entryPoints);

	// Second - Hit Program - link to entry point names
	CD3DX12_HIT_GROUP_SUBOBJECT hitSubObject(stateObjectDesc);
	hitSubObject.SetClosestHitShaderImport(L"chs");
	hitSubObject.SetHitGroupExport(L"HitGroup");

	// Third - Local Root Signature for Ray Gen shader
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0)); //gOutput, gRadiance
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 10)); //gRtScene
	if (!rendererResources->textures.empty())
		rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(rendererResources->textures.size()), 12));

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

	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootSignatureManager->addParametersToRootSignature("RayGenRootSignature", { "BVHDescTable", "ConstBuff", "verts", "texVerts", "normals", "indices", "matrices", "normalMatrices", "faceAttributes", "materials", "lights" });
	rootSignatureManager->setSamplerForRootSignature("RayGenRootSignature", sampler);
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
	LPCWSTR exports[] = { L"rayGen", L"miss", L"HitGroup", L"shadowMiss" };
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
	shadingTable->addProgram(L"miss", ShadingRecordType::Miss, "EmptyRootSignature");
	shadingTable->addProgram(L"shadowMiss", ShadingRecordType::Miss, "EmptyRootSignature");
	shadingTable->addProgram(L"HitGroup", ShadingRecordType::HitGroup, "EmptyRootSignature");
	shadingTable->addProgramAssociationsToSubobject(stateObjectDesc);
	wrl::ComPtr<ID3DBlob> pBlob;
	GFXTHROWIFFAILED(D3DReadFileToBlob(StringToWString("./Shaders/PathTracingShader.cso").c_str(), &pBlob));
	auto shaderByteCodeDesc = CD3DX12_SHADER_BYTECODE(pBlob.Get());
	dxilSubObject.SetDXILLibrary(&shaderByteCodeDesc);
	GFXTHROWIFFAILED(pDevice5->CreateStateObject(stateObjectDesc, IID_PPV_ARGS(&stateObject)));
}

void PathTracingShading::createShaderResources()
{
	size_t entryNumber = 0;

	// The descriptor heap to store SRV (Shader resource View) and UAV (Unordered access view) descriptors
	auto &descHeapManager = shadingTable->generateDescriptorHeap("BVHDescTable", "BVH1", rendererResources->pDevice);
	descriptorHeap = descHeapManager.getDescriptorHeap();
	const auto& dim = rendererResources->winDimensions;

	// The output resource
	radianceTexture = make_unique<Resource>(Resource::createTextureCommittedResource(
		rendererResources->pDevice, dim.x, dim.y,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
	radianceTexture->setName(L"Radiance Texture");

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	descHeapManager.setUAV(entryNumber++, uavDesc, rendererResources->pDevice, *rendererResources->pRTVRadBackBuffer);
	descHeapManager.setUAV(entryNumber++, uavDesc, rendererResources->pDevice, *radianceTexture);

	// Create the SRV descriptor in second place (following same order as in root signature)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = rendererResources->accelerationStructure->getTopLayerBufferAddress();
	descHeapManager.setSRV(entryNumber++, srvDesc, rendererResources->pDevice);

	// Textures
	srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	for (const auto& texture : rendererResources->textures)
		descHeapManager.setSRV(entryNumber++, srvDesc, rendererResources->pDevice, texture);
}

void PathTracingShading::createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList, wrl::ComPtr<ID3D12Resource>& tempBuffer)
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
	shadingTable->generateShadingTable(rendererResources->pDevice, commandList, stateObject, tempBuffer);
}
