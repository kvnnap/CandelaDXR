#include "LightTracingShading.h"

#include <DirectXMath.h>
#include <d3dcompiler.h>

#include "DirectX/d3dx12.h"

#include "DirectX/DxUtil.h"

#include "Exception/WindowException.h"

#include "Util/StringUtil.h"

#include "AccelerationStructure.h"

using std::uint32_t;
using std::vector;
using std::string;
using std::wstring;
using std::unique_ptr;
using std::make_unique;
using std::make_shared;

using DirectX::XMFLOAT3X4;

using Microsoft::WRL::ComPtr;

using candela::directx::DXUtil;
using candela::directx::RootSignatureManager;
using candela::directx::ShadingTable;
using candela::directx::DescriptorHeap;
using candela::directx::ShadingRecordType;

using candela::renderer::LightTracingShading;
using candela::renderer::RendererResources;
using candela::renderer::ChangeEvent;
using candela::renderer::ChangeEvent_t;
using candela::renderer::ResourceRegFunction;
using candela::renderer::AccelerationStructure;

using candela::mathematics::UVector2;
using candela::mathematics::Vector2;
using candela::mathematics::Vector3;

using candela::sampler::ISampler;

using candela::util::StringToWString;

LightTracingShading::LightTracingShading(unique_ptr<ISampler> sampler, UVector2 lightSamples)
	: rendererResources(), constBuffer(), lightSamples(lightSamples), sampler(std::move(sampler)), clear(), currentShader()
{
}

void LightTracingShading::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList> &pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	if (!DXUtil::checkDeviceRTSupport(rRes->pDevice))
		ThrowException("Ray tracing is not supported on this device");
	
	if (!rRes->accelerationStructure)
	{
		auto accel = make_unique<AccelerationStructure>();
		rRes->accelerationStructure = accel.get();
		resRegFn(std::move(accel));
	}

	rendererResources = rRes;

	shaderPaths.emplace_back("./Shaders/LightTracingShader.cso");
	shaderPaths.emplace_back("./Shaders/LightTracingOptimisedShader.cso");
	shaderPaths.emplace_back("./Shaders/PathTracingShader.cso");
	constantTempBuffer.resize(rRes->numBackBuffers);
	constBuffer.numLights = static_cast<uint32_t>(rRes->scene->getLights().size());
	constBuffer.numSpeculars = static_cast<uint32_t>(rRes->scene->getSpeculars().size());
	constBuffer.frameNumber = 0;
	constBuffer.causticsRatio = 0.f;
	constBuffer.pathFilter = 0xFFFFFFFF;
	auto& scene = *rRes->scene;

	// Gen 
	generateIrrToRadTexture(pCurrentCommandList, rendererResources->initTempBuffers.emplace_back());

	// Build Pipeline
	buildPipeline();

	// Create Shader resources
	createShaderResources();

	// Constant buffer
	constantBuffer = DXUtil::uploadDataToDefaultHeap(rRes->pDevice, pCurrentCommandList, rendererResources->initTempBuffers.emplace_back(), &constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	constantBuffer->SetName(L"Constant Buffer");

	// Build shading table
	shadingTableTempBuffers.resize(shaderPaths.size());
	createShaderTable(pCurrentCommandList);
}

void LightTracingShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList, uint32_t currentBackBufferIndex)
{
	// Pre-stuff
	auto &backBuff = rendererResources->pRTVRadBackBuffers[currentBackBufferIndex];
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuff.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	currentCommandList->ResourceBarrier(1u, &barrier);
	
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(outputTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	currentCommandList->ResourceBarrier(1u, &barrier);

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
	DXUtil::updateDataInDefaultHeap(rendererResources->pDevice, currentCommandList, constantBuffer, constantTempBuffer[currentBackBufferIndex],
		&constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	currentCommandList->SetDescriptorHeaps(1u, descriptorHeap.GetAddressOf());
	currentCommandList->SetComputeRootSignature(globalEmptyRootSignature.Get());
	HRESULT hr;
	ComPtr<ID3D12GraphicsCommandList4> commandList4;
	GFXTHROWIFFAILED(currentCommandList.As(&commandList4));
	commandList4->SetPipelineState1(stateObjects[currentShader].Get());

	// Launch rays
	auto rayDimensions = currentShader == 2 || lightSamples.x == 0 || lightSamples.y == 0 ? rendererResources->winDimensions : lightSamples;
	D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = shadingTables[currentShader]->getDispatchRaysDescriptor(rayDimensions.x, rayDimensions.y);
	commandList4->DispatchRays(&dispatchRaysDesc);

	if (currentShader < 2)
	{
		// Launch compute shader -  Make sure all writes to this UAV have completed from DispatchRays
		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(irradianceDataStructure.Get());
		currentCommandList->ResourceBarrier(1u, &uavBarrier);

		currentCommandList->SetPipelineState(computePipelineState.Get());
		currentCommandList->SetComputeRootSignature(computeRootSignature.Get());
		currentCommandList->SetDescriptorHeaps(1u, computeDescriptorHeap.GetAddressOf());
		currentCommandList->SetComputeRootDescriptorTable(0u, computeDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		auto& dim = rendererResources->winDimensions;
		uint32_t c32data[5] = { dim.x, dim.y, rayDimensions.x * rayDimensions.y, constBuffer.frameNumber, clear ? 1u : 0u };
		currentCommandList->SetComputeRoot32BitConstants(1u, 5u, &c32data[0], 0);
		currentCommandList->Dispatch(dim.x / 8 + (dim.x % 8 == 0 ? 0 : 1), dim.y / 8 + (dim.y % 8 == 0 ? 0 : 1), 1);
	}
	clear = false;

	// After
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(outputTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	currentCommandList->ResourceBarrier(1u, &barrier);
	currentCommandList->CopyResource(backBuff.Get(), outputTexture.Get());
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuff.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
	currentCommandList->ResourceBarrier(1u, &barrier);
}

void LightTracingShading::buildPipeline()
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
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0)); //gOutput, gIrradiance
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 10)); //gRtScene, gIrrToRad
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
	param.InitAsShaderResourceView(9); rootSignatureManager->setParameter("speculars", param);

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

	rootSignatureManager->addParametersToRootSignature("RayGenRootSignature", { "BVHDescTable", "ConstBuff", "verts", "texVerts", "normals", "indices", "matrices", "normalMatrices", "faceAttributes", "materials", "lights", "speculars"});
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
	bool isFirst = true;
	for (const auto& shaderPath : shaderPaths)
	{
		auto& shadingTable = shadingTables.emplace_back(make_unique<ShadingTable>(rootSignatureManager));
		shadingTable->addProgram(L"rayGen", ShadingRecordType::RayGeneration, "RayGenRootSignature");
		shadingTable->addProgram(L"miss", ShadingRecordType::Miss, "EmptyRootSignature");
		shadingTable->addProgram(L"shadowMiss", ShadingRecordType::Miss, "EmptyRootSignature");
		shadingTable->addProgram(L"HitGroup", ShadingRecordType::HitGroup, "EmptyRootSignature");
		if (isFirst)
			shadingTable->addProgramAssociationsToSubobject(stateObjectDesc);
		isFirst = false;
		wrl::ComPtr<ID3DBlob> pBlob;
		GFXTHROWIFFAILED(D3DReadFileToBlob(StringToWString(shaderPath).c_str(), &pBlob));
		auto shaderByteCodeDesc = CD3DX12_SHADER_BYTECODE(pBlob.Get());
		dxilSubObject.SetDXILLibrary(&shaderByteCodeDesc);
		GFXTHROWIFFAILED(pDevice5->CreateStateObject(stateObjectDesc, IID_PPV_ARGS(&stateObjects.emplace_back())));
	}

	// Compute shader
	computeRSM = make_shared<RootSignatureManager>();
	computeRSM->addDescriptorRange("ComputeData", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0)); // gOutput, gIrradianceDataStructure, gIrradiance
	computeRSM->addDescriptorRange("ComputeData", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0)); // gIrrToRad
	computeRSM->setDescriptorTableParameter("ComputeDataDescTable", "ComputeData");
	param.InitAsConstants(5u, 0u); computeRSM->setParameter("ComputeConstants", param); // winDimensions (x,y), lightSamples, numFrames, clear
	computeRSM->addParametersToRootSignature("ComputeRootSignature", { "ComputeDataDescTable", "ComputeConstants" });
	computeRootSignature = computeRSM->generateRootSignature("ComputeRootSignature", rendererResources->pDevice);

	// Get shader
	wrl::ComPtr<ID3DBlob> pComputeBlob;
	GFXTHROWIFFAILED(D3DReadFileToBlob(L"./Shaders/RadianceComputeShader.cso", &pComputeBlob));

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
	GFXTHROWIFFAILED(pDevice5->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&computePipelineState)));
}

void LightTracingShading::createShaderResources()
{
	size_t entryNumber = 0;

	// The descriptor heap to store SRV (Shader resource View) and UAV (Unordered access view) descriptors
	auto descHeapManager = make_shared<DescriptorHeap>(rootSignatureManager, "BVHDescTable", "BVH1", rendererResources->pDevice);

	// Add to shading tables
	for (auto& shadingTable : shadingTables)
		shadingTable->addDescriptorHeap(descHeapManager);

	descriptorHeap = descHeapManager->getDescriptorHeap();

	const auto &dim = rendererResources->winDimensions;

	// The output resource
	outputTexture = DXUtil::createTextureCommittedResource(rendererResources->pDevice, D3D12_HEAP_TYPE_DEFAULT, dim.x, dim.y, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT);
	outputTexture->SetName(L"Output Texture");
	irradianceTexture = DXUtil::createTextureCommittedResource(rendererResources->pDevice, D3D12_HEAP_TYPE_DEFAULT, dim.x, dim.y, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT);
	irradianceTexture->SetName(L"Irradiance Texture");
	irradianceDataStructure = DXUtil::createCommittedResource(rendererResources->pDevice, D3D12_HEAP_TYPE_DEFAULT, dim.x * dim.y * sizeof(uint32_t) * 4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	irradianceDataStructure->SetName(L"Irradiance DS");

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	descHeapManager->setUAV(entryNumber++, uavDesc, rendererResources->pDevice, outputTexture);
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc2 = {};
	uavDesc2.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc2.Buffer.NumElements = dim.x * dim.y;
	uavDesc2.Buffer.StructureByteStride = sizeof(uint32_t) * 4;
	descHeapManager->setUAV(entryNumber++, uavDesc2, rendererResources->pDevice, irradianceDataStructure);

	// Create the SRV descriptor in second place (following same order as in root signature)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = rendererResources->accelerationStructure->getTopLayerBufferAddress();
	descHeapManager->setSRV(entryNumber++, srvDesc, rendererResources->pDevice);

	// Irr to Rad?
	D3D12_SHADER_RESOURCE_VIEW_DESC irrToRadSrvDesc = {};
	irrToRadSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	irrToRadSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	irrToRadSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	irrToRadSrvDesc.Texture2D.MipLevels = 1;
	descHeapManager->setSRV(entryNumber++, irrToRadSrvDesc, rendererResources->pDevice, irrToRad);

	// Textures
	srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	for (const auto& texture : rendererResources->textures)
		descHeapManager->setSRV(entryNumber++, srvDesc, rendererResources->pDevice, texture);

	// Compute shader
	auto cmpDescHeapManager = DescriptorHeap(computeRSM, "ComputeDataDescTable", "ComputeData1", rendererResources->pDevice);
	cmpDescHeapManager.setUAV(0, uavDesc, rendererResources->pDevice, outputTexture);
	cmpDescHeapManager.setUAV(1, uavDesc2, rendererResources->pDevice, irradianceDataStructure);
	cmpDescHeapManager.setUAV(2, uavDesc, rendererResources->pDevice, irradianceTexture);
	cmpDescHeapManager.setSRV(3, irrToRadSrvDesc, rendererResources->pDevice, irrToRad);
	computeDescriptorHeap = cmpDescHeapManager.getDescriptorHeap();
}

void LightTracingShading::createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList> &commandList)
{
	for (size_t i = 0; i < shadingTables.size(); ++i)
	{
		auto &shadingTable = shadingTables[i];
		
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
		shadingTable->setInputForViewParameter(L"rayGen", "speculars", rendererResources->specularBuffer);

		// Generate
		shadingTable->generateShadingTable(rendererResources->pDevice, commandList, stateObjects[i], shadingTableTempBuffers[i]);
	}
}

void LightTracingShading::generateIrrToRadTexture(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList, wrl::ComPtr<ID3D12Resource>& tempResource)
{
	// Compute irradianceToRadianceConstants
	const auto& dim = rendererResources->winDimensions;
	vector<float> irradianceToRadianceConstants;
	irradianceToRadianceConstants.reserve(dim.x * dim.y);
	for (uint32_t y = 0; y < dim.y; ++y)
		for (uint32_t x = 0; x < dim.x; ++x)
			irradianceToRadianceConstants.push_back(1.f / cosIntegral(x, y));
	irrToRad = DXUtil::uploadTextureDataToDefaultHeap(rendererResources->pDevice, commandList, tempResource, irradianceToRadianceConstants.data(),
		dim.x, dim.y, sizeof(float), DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	irrToRad->SetName(L"Irr-to-rad Texture");
}

void LightTracingShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::SceneChange))
	{
		constBuffer.numLights = static_cast<uint32_t>(rendererResources->scene->getLights().size());
		constBuffer.numSpeculars = static_cast<uint32_t>(rendererResources->scene->getSpeculars().size());
		createShaderTable(pCurrentCommandList);
	}
	clear = true;
}

void LightTracingShading::onResize()
{
	auto commandList = rendererResources->commandQueue->getCommandList();
	wrl::ComPtr<ID3D12Resource> irrToRadTempBuffer;
	generateIrrToRadTexture(commandList, irrToRadTempBuffer);
	createShaderResources();
	clear = true;
	// Wait
	auto fV = rendererResources->commandQueue->executeCommandList(commandList);
	rendererResources->commandQueue->waitForFenceValue(fV);
}

void LightTracingShading::accept(IVisitor* visitor)
{
	visitor->visit(this);
}

const UVector2& LightTracingShading::getLightSamples() const
{
	return lightSamples;
}

void LightTracingShading::setLightSamples(const UVector2& p_lightSamples)
{
	lightSamples = p_lightSamples;
}

const vector<string>& LightTracingShading::getShaderPaths() const
{
	return shaderPaths;
}

void LightTracingShading::setCurrentShaderIndex(uint32_t currentShaderIndex)
{
	currentShader = currentShaderIndex;
}

uint32_t LightTracingShading::getCurrentShaderIndex() const
{
	return currentShader;
}

void LightTracingShading::setCausticsRatio(float p_causticsRatio)
{
	constBuffer.causticsRatio = p_causticsRatio;
}

float LightTracingShading::getCausticsRatio() const
{
	return constBuffer.causticsRatio;
}

uint32_t LightTracingShading::getPathFilter() const
{
	return constBuffer.pathFilter;
}

void LightTracingShading::setPathFilter(uint32_t pathFilter)
{
	constBuffer.pathFilter = pathFilter;
}

// Compute constants
static float f1(float x, float y, float z, float a, float b, float c)
{
	float xMinA = x - a;
	float yMinB = y - b;
	float zMinC = z - c;

	float xMinASq = xMinA * xMinA;
	float yMinBSq = yMinB * yMinB;
	float zMinCSq = zMinC * zMinC;

	float r1 = 1.f / sqrt(yMinBSq + zMinCSq);
	float r2 = 1.f / sqrt(xMinASq + zMinCSq);

	return 0.5f * (
		yMinB * atan(xMinA * r1) * r1 +
		xMinA * atan(yMinB * r2) * r2);
}

static float f(float x0, float x1, float y0, float y1, float z, float a, float b, float c)
{
	return f1(x1, y1, z, a, b, c)
		 + f1(x0, y0, z, a, b, c)
		 - f1(x1, y0, z, a, b, c)
		 - f1(x0, y1, z, a, b, c);
}

Vector2 LightTracingShading::toSensorSpace(uint32_t x, uint32_t y) const
{
	UVector2 &screenDimensions = rendererResources->winDimensions;
	auto nd = rendererResources->camera->getNearPlaneDimensions();
	Vector2 sensorDimensions = Vector2(nd.m128_f32[0], nd.m128_f32[1]);
	const auto temp = Vector2(static_cast<float>(x), static_cast<float>(screenDimensions.y - y));
	using namespace DirectX;
	auto point = XMLoadFloat2(&temp);
	point = XMVectorDivide(point, XMLoadUInt2(&screenDimensions)); // Ratio
	point = XMVectorSubtract(point, XMVectorSet(0.5f, 0.5f, 0.f, 0.f)); // Center it
	point = XMVectorMultiply(point, XMLoadFloat2(&sensorDimensions)); // This point now lies in sensor space
	Vector2 result;
	XMStoreFloat2(&result, point);
	return result;
}

float LightTracingShading::cosIntegral(uint32_t x, uint32_t y) const
{
	auto pt0 = toSensorSpace(x, y + 1); // Min
	auto pt1 = toSensorSpace(x + 1, y); // Max

	return f(pt0.x, pt1.x, pt0.y, pt1.y, rendererResources->camera->getNearPlaneDimensions().m128_f32[2], 0.f, 0.f, 0.f);
}