#include "LightTracingShading.h"
#include "LTOptimisedComponent.h"
#include "LTRasterGuidedShading.h"

#include <DirectXMath.h>
#include <d3dcompiler.h>

#include "DirectX/d3dx12.h"

#include "DirectX/DxUtil.h"

#include "Exception/WindowException.h"

#include "Util/StringUtil.h"
#include "Mathematics/Utils.h"

#include "AccelerationStructure.h"
#include "RendererResources.h"

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
using candela::directx::Resource;
using candela::directx::DXResource;
using candela::directx::DXCommandList;

using candela::renderer::LightTracingShading;
using candela::renderer::ILightTracingComponent;
using candela::renderer::LTOptimisedComponent;
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
	: rendererResources(), constBuffer(), lightSamples(lightSamples),
	irradianceDataStructure(), irrToRad(), prngState(), rayHitT(), irradianceCaustics(),
	irradianceTexture(), sampler(std::move(sampler)), frameNumberCaustics(),
	clear(), clearCaustics(), allowClearCaustics(true), currentShader()
{
}

void LightTracingShading::init(RendererResources* rRes, DXCommandList &pCurrentCommandList, ResourceRegFunction& resRegFn)
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
	
	constBuffer.numLights = static_cast<uint32_t>(rRes->scene->getLights().size());
	constBuffer.numTotalLights = constBuffer.numLights + static_cast<uint32_t>(rRes->scene->getExternalLights().size());
	constBuffer.frameNumber = 0;
	constBuffer.seeds[0] = sampler->nextUInt32();
	constBuffer.seeds[1] = 1u; // Set to one so the shader consumes this seed
	constBuffer.pathFilter = 0xFFFFFFFF;
	auto& scene = *rRes->scene;

	// Testing central resources
	const auto& dim = rendererResources->winDimensions;
	irradianceTexture = &rendererResources->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "lt_irr");
	irradianceTexture->setName(L"Irradiance Texture");
	prngState = &rendererResources->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32_UINT, true);
	prngState->setName(L"prngState Texture");
	irrToRad = &rendererResources->resourceManager->createResource(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_NONE, dim.x, dim.y, DXGI_FORMAT_R32_FLOAT, true);
	irrToRad->setName(L"irrToRad Texture");
	rayHitT = &rRes->resourceManager->createResourceIfNotExists(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		rRes->winDimensions.x, rRes->winDimensions.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "ray_hitT");
	irradianceCaustics = &rendererResources->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, DXGI_FORMAT_R32G32B32A32_FLOAT, true, "lt_irr_caustics");

	// Init components
	for (auto& ltShader : ltShaders)
	{
		if (ltShader.component)
			ltShader.component->init(rRes, pCurrentCommandList, resRegFn);
	}

	// Gen 
	generateIrrToRadTexture(pCurrentCommandList, rendererResources->getTempResource());

	// Build Pipeline
	buildPipeline();

	// Create Shader resources
	createShaderResources(pCurrentCommandList);
	
	// Constant buffer
	constantBuffer = DXUtil::uploadDataToDefaultHeap(rRes->pDevice, pCurrentCommandList, rendererResources->getTempResource(), &constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	constantBuffer->SetName(L"LT Constant Buffer");

	// Build shading table
	createShaderTable(pCurrentCommandList);

	// To blur caustics
	guassResources = { 
		rRes->pRTVCaus, // this is the input AND output
		irradianceCaustics // This is a dummy
	};
	guassianCS.setDxgiFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	guassianCS.init(rendererResources, pCurrentCommandList, &guassResources);
	guassianCS.setOutputTexture(0u);
	guassianCS.setInputTexture(static_cast<uint32_t>(guassResources.size() - 1));
	//guassianCS.setFiltersize(17);
}

void LightTracingShading::draw(DXCommandList currentCommandList, uint32_t currentBackBufferIndex)
{
	auto& ltShader = ltShaders[currentShader];

	// Draw component
	if (ltShader.component)
		ltShader.component->draw(currentCommandList, currentBackBufferIndex);

	// Pre-stuff
	auto& backBuff = rendererResources->pRTVDiff;
	auto &caustBuff = rendererResources->pRTVCaus;
	backBuff->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	if (constBuffer.seperateCaustics && guassianCS.getFiltersize() > 1)
		caustBuff->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// Copy and update camera
	auto cam = rendererResources->camera;
	constBuffer.w = DirectX::XMVector3Normalize(cam->getDirection());
	constBuffer.u = DirectX::XMVectorNegate(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cam->getUp(), cam->getDirection())));
	constBuffer.v = DirectX::XMVectorNegate(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(constBuffer.w, constBuffer.u)));
	constBuffer.position = cam->getPosition();
	constBuffer.direction = cam->getDirection();
	constBuffer.plane = cam->getNearPlaneDimensions();
	constBuffer.winDimensions = rendererResources->winDimensions;
	if (clear)
		constBuffer.frameNumber = 0;
	if (clearCaustics)
		frameNumberCaustics = 0;
	++constBuffer.frameNumber;
	++frameNumberCaustics;
	DXUtil::updateDataInDefaultHeap(rendererResources->pDevice, currentCommandList, constantBuffer, rendererResources->getTempResource(),
		&constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	currentCommandList->SetDescriptorHeaps(1u, ltShader.descHeapManager->getDescriptorHeap().GetAddressOf());
	currentCommandList->SetComputeRootSignature(globalEmptyRootSignature.Get());
	HRESULT hr;
	ComPtr<ID3D12GraphicsCommandList4> commandList4;
	GFXTHROWIFFAILED(currentCommandList.As(&commandList4));
	commandList4->SetPipelineState1(ltShader.stateObject.Get());

	// Launch rays
	auto rayDimensions = lightSamples.x == 0 || lightSamples.y == 0 ? rendererResources->winDimensions : lightSamples;
	D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = ltShader.shadingTable->getDispatchRaysDescriptor(rayDimensions.x, rayDimensions.y);
	commandList4->DispatchRays(&dispatchRaysDesc);

	// Launch compute shader -  Make sure all writes to this UAV have completed from DispatchRays
	irradianceDataStructure->uavBarrier(currentCommandList);
	
	currentCommandList->SetComputeRootSignature(computeRootSignature.Get());
	currentCommandList->SetPipelineState(computePipelineState.Get());
	currentCommandList->SetDescriptorHeaps(1u, computeDescriptorHeap.GetAddressOf());
	currentCommandList->SetComputeRootDescriptorTable(0u, computeDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	auto& dim = rendererResources->winDimensions;
	uint32_t c32data[8] = { dim.x, dim.y, rayDimensions.x * rayDimensions.y, constBuffer.frameNumber, clear ? 1u : 0u, frameNumberCaustics, clearCaustics ? 1u : 0u, constBuffer.rangeBits };
	currentCommandList->SetComputeRoot32BitConstants(1u, static_cast<UINT>(std::size(c32data)), &c32data[0], 0);
	currentCommandList->Dispatch(dim.x / 8 + (dim.x % 8 == 0 ? 0 : 1), dim.y / 8 + (dim.y % 8 == 0 ? 0 : 1), 1);
	clear = clearCaustics = false;
	constBuffer.seeds[1] = 0u;

	// Caustics blur
	if (constBuffer.seperateCaustics && guassianCS.getFiltersize() > 1)
	{
		guassianCS.compute(currentCommandList);
		caustBuff->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	// After
	backBuff->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

vector<LightTracingShading::LTShaderInfo> LightTracingShading::getLTShaderInfo()
{
	vector<LTShaderInfo> out;
	std::transform(ltShaders.begin(), ltShaders.end(), std::back_inserter(out), [](LTShader& ltShader) {
		return LTShaderInfo{ .shaderPath = &ltShader.shaderPath, .component = ltShader.component.get() };
	});
	return out;
}

void LightTracingShading::addLtShader(const std::string& shaderPath, std::unique_ptr<ILightTracingComponent> component)
{
	ltShaders.emplace_back(LTShader{shaderPath, move(component)});
}

void LightTracingShading::buildPipeline()
{
	HRESULT hr;

	ComPtr<ID3D12Device5> pDevice5;
	GFXTHROWIFFAILED(rendererResources->pDevice.As(&pDevice5));

	// Global empty RS
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC globalEmptyRootSignatureDesc(0, static_cast<CD3DX12_ROOT_PARAMETER1*>(nullptr), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
	globalEmptyRootSignature = DXUtil::createRootSignature(rendererResources->pDevice, globalEmptyRootSignatureDesc);
	globalEmptyRootSignature->SetName(L"Root Signature Global");

	CD3DX12_ROOT_PARAMETER1 param;

	// Third - Local Root Signature for Ray Gen shader
	for (auto& ltShader : ltShaders)
	{
		// Define State Object Descriptor (EXTENDED version from d3dx)
		CD3DX12_STATE_OBJECT_DESC stateObjectDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

		// First is DXIL - to load shader and Load symbols from the shader and identify the entry points
		CD3DX12_DXIL_LIBRARY_SUBOBJECT dxilSubObject(stateObjectDesc);
		const WCHAR* entryPoints[] = { L"rayGen", L"miss", L"chs", L"shadowMiss" };
		dxilSubObject.DefineExports(entryPoints);

		// Second - Hit Program - link to entry point names
		CD3DX12_HIT_GROUP_SUBOBJECT hitSubObject(stateObjectDesc);
		hitSubObject.SetClosestHitShaderImport(L"chs");
		hitSubObject.SetHitGroupExport(L"HitGroup");

		ltShader.rootSignatureManager = make_shared<RootSignatureManager>();
		auto rootSignatureManager = ltShader.rootSignatureManager;

		rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0)); //gIrradianceDS
		rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 11)); //gRtScene
		if (!rendererResources->textures.empty())
			rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(rendererResources->textures.size()), 12));

		rootSignatureManager->setDescriptorTableParameter("BVHDescTable", "BVH");
		param.InitAsConstantBufferView(0); rootSignatureManager->setParameter("ConstBuff", param);
		param.InitAsShaderResourceView(0); rootSignatureManager->setParameter("verts", param);
		param.InitAsShaderResourceView(1); rootSignatureManager->setParameter("texVerts", param);
		param.InitAsShaderResourceView(2); rootSignatureManager->setParameter("normals", param);
		param.InitAsShaderResourceView(3); rootSignatureManager->setParameter("indices", param);
		param.InitAsShaderResourceView(4); rootSignatureManager->setParameter("matrices", param);
		param.InitAsShaderResourceView(5); rootSignatureManager->setParameter("normalMatrices", param);
		param.InitAsShaderResourceView(6); rootSignatureManager->setParameter("faceAttributes", param);
		param.InitAsShaderResourceView(7); rootSignatureManager->setParameter("materials", param);
		param.InitAsShaderResourceView(8); rootSignatureManager->setParameter("lights", param);
		param.InitAsShaderResourceView(9); rootSignatureManager->setParameter("eLights", param);
		param.InitAsShaderResourceView(10); rootSignatureManager->setParameter("speculars", param);

		rootSignatureManager->addParametersToRootSignature("RayGenRootSignature", { "BVHDescTable", "ConstBuff", "verts", "texVerts", "normals", "indices", "matrices", "normalMatrices", "faceAttributes", "materials", "lights", "eLights", "speculars" });
		rootSignatureManager->setSamplerForRootSignature("RayGenRootSignature", DXUtil::getDefaultSamplerDesc());
		rootSignatureManager->addRootSignature("EmptyRootSignature");

		if (ltShader.component)
			ltShader.component->appendToPipeline(rootSignatureManager.get());

		// Gen root signatures
		rootSignatureManager->generateRootSignature("RayGenRootSignature", rendererResources->pDevice);
		rootSignatureManager->generateRootSignature("EmptyRootSignature", rendererResources->pDevice);

		// Generate/add subobjects
		rootSignatureManager->addRootSignaturesToSubObject(stateObjectDesc);

		// Seventh - Shader Configuration (set payload sizes - the actual program parameters)
		CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT shaderConfig(stateObjectDesc);
		shaderConfig.Config(5 * sizeof(float), 2 * sizeof(float));

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
		globalRootSignature.SetRootSignature(globalEmptyRootSignature.Get());

		// Finally - Create the state
		
		ltShader.shadingTable = make_unique<ShadingTable>(rootSignatureManager);

		auto& shadingTable = ltShader.shadingTable;
		shadingTable->addProgram(L"rayGen", ShadingRecordType::RayGeneration, "RayGenRootSignature");
		shadingTable->addProgram(L"miss", ShadingRecordType::Miss, "EmptyRootSignature");
		shadingTable->addProgram(L"shadowMiss", ShadingRecordType::Miss, "EmptyRootSignature");
		shadingTable->addProgram(L"HitGroup", ShadingRecordType::HitGroup, "EmptyRootSignature");
		shadingTable->addProgramAssociationsToSubobject(stateObjectDesc);
		
		wrl::ComPtr<ID3DBlob> pBlob = DXUtil::LoadShaderResource(ltShader.shaderPath.c_str());
		auto shaderByteCodeDesc = CD3DX12_SHADER_BYTECODE(pBlob.Get());
		dxilSubObject.SetDXILLibrary(&shaderByteCodeDesc);
		GFXTHROWIFFAILED(pDevice5->CreateStateObject(stateObjectDesc, IID_PPV_ARGS(&ltShader.stateObject)));
	}
	
	// Compute shader
	computeRSM = make_shared<RootSignatureManager>();
	computeRSM->addDescriptorRange("ComputeData", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 6, 0)); // gOutput, gIrradianceDataStructure, gIrradiance, rayhitt, irrC, outC
	computeRSM->addDescriptorRange("ComputeData", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0)); // gIrrToRad
	computeRSM->setDescriptorTableParameter("ComputeDataDescTable", "ComputeData");
	param.InitAsConstants(8u, 0u); computeRSM->setParameter("ComputeConstants", param); // winDimensions (x,y), lightSamples, numFrames, clear
	computeRSM->addParametersToRootSignature("ComputeRootSignature", { "ComputeDataDescTable", "ComputeConstants" });
	computeRootSignature = computeRSM->generateRootSignature("ComputeRootSignature", rendererResources->pDevice, D3D12_ROOT_SIGNATURE_FLAG_NONE);

	// Get shader
	wrl::ComPtr<ID3DBlob> pComputeBlob = DXUtil::LoadShaderResource("./Shaders/RadianceComputeShader.cso");

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

void LightTracingShading::createShaderResources(DXCommandList& commandList)
{
	const auto& dim = rendererResources->winDimensions;

	// Uav Desc 
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc2 = {};
	uavDesc2.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc2.Buffer.NumElements = dim.x * dim.y;
	uavDesc2.Buffer.StructureByteStride = sizeof(uint32_t) * 4 * 2; // IrradianceItem - val and caustics

	// The output resource
	irradianceDataStructure = &rendererResources->resourceManager->createResource(D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, uavDesc2.Buffer.NumElements * uavDesc2.Buffer.StructureByteStride);
	irradianceDataStructure->setName(L"Irradiance DS");
	irradianceDataStructure->transistionBarrier(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	for (auto& ltShader : ltShaders)
	{
		auto &descHeapManager = ltShader.descHeapManager;

		// If heap already exists, do not recreate and do not modify shading tables
		if (!descHeapManager)
		{
			// The descriptor heap to store SRV (Shader resource View) and UAV (Unordered access view) descriptors
			descHeapManager = make_shared<DescriptorHeap>(ltShader.rootSignatureManager, "BVHDescTable", "BVH1", rendererResources->pDevice);

			// Add to shading tables
			ltShader.shadingTable->setDescriptorHeapIfNotExists(descHeapManager);
		}
		
		// Set irradiance DS UAV
		size_t entryNumber{};
		//uavDesc.Format = DXGI_FORMAT_R32_UINT;
		descHeapManager->setUAV(entryNumber++, uavDesc, rendererResources->pDevice, *prngState);
		descHeapManager->setUAV(entryNumber++, uavDesc2, rendererResources->pDevice, *irradianceDataStructure);

		// Create the SRV descriptor in second place (following same order as in root signature)
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.RaytracingAccelerationStructure.Location = rendererResources->accelerationStructure->getTopLayerBufferAddress();
		descHeapManager->setSRV(entryNumber++, srvDesc, rendererResources->pDevice);

		// Textures
		srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		for (const auto& texture : rendererResources->textures)
		{
			const directx::DXResource& texRes = texture;
			srvDesc.Format = texRes->GetDesc().Format;
			descHeapManager->setSRV(entryNumber++, srvDesc, rendererResources->pDevice, texture);
		}

		if (ltShader.component)
			ltShader.component->appendToDescHeapManager(descHeapManager.get());
	}

	// Irr to Rad
	D3D12_SHADER_RESOURCE_VIEW_DESC irrToRadSrvDesc = {};
	irrToRadSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	irrToRadSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	irrToRadSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	irrToRadSrvDesc.Texture2D.MipLevels = 1;

	// Compute shader
	auto cmpDescHeapManager = DescriptorHeap(computeRSM, "ComputeDataDescTable", "ComputeData1", rendererResources->pDevice);
	cmpDescHeapManager.setUAV(0, uavDesc, rendererResources->pDevice, *rendererResources->pRTVDiff);
	cmpDescHeapManager.setUAV(1, uavDesc2, rendererResources->pDevice, *irradianceDataStructure);
	cmpDescHeapManager.setUAV(2, uavDesc, rendererResources->pDevice, *irradianceTexture);
	cmpDescHeapManager.setUAV(3, uavDesc, rendererResources->pDevice, *rayHitT);
	cmpDescHeapManager.setUAV(4, uavDesc, rendererResources->pDevice, *irradianceCaustics);
	cmpDescHeapManager.setUAV(5, uavDesc, rendererResources->pDevice, *rendererResources->pRTVCaus);
	cmpDescHeapManager.setSRV(6, irrToRadSrvDesc, rendererResources->pDevice, *irrToRad);
	computeDescriptorHeap = cmpDescHeapManager.getDescriptorHeap();
}

void LightTracingShading::createShaderTable(DXCommandList &commandList)
{
	for (auto& ltShader : ltShaders)
	{
		auto &shadingTable = ltShader.shadingTable;
		
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
		shadingTable->setInputForViewParameter(L"rayGen", "eLights", rendererResources->externalLights);
		shadingTable->setInputForViewParameter(L"rayGen", "speculars", rendererResources->specularBuffer);

		// Append components
		if (ltShader.component)
			ltShader.component->appendToShaderTable(shadingTable.get());

		// Generate
		shadingTable->generateShadingTable(rendererResources->pDevice, commandList, ltShader.stateObject, rendererResources->getTempResource());
	}
}

void LightTracingShading::generateIrrToRadTexture(DXCommandList& commandList, DXResource& tempResource)
{
	// Compute irradianceToRadianceConstants
	const auto& dim = rendererResources->winDimensions;
	vector<float> irradianceToRadianceConstants;
	irradianceToRadianceConstants.reserve(dim.x * dim.y);
	for (uint32_t y = 0; y < dim.y; ++y)
		for (uint32_t x = 0; x < dim.x; ++x)
			irradianceToRadianceConstants.push_back(cosIntegral(x, y));
	irrToRad->write(commandList, tempResource, irradianceToRadianceConstants.data());
}

void LightTracingShading::onChange(DXCommandList pCurrentCommandList, uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::SceneChange))
	{
		constBuffer.numLights = static_cast<uint32_t>(rendererResources->scene->getLights().size());
		constBuffer.numTotalLights = constBuffer.numLights + static_cast<uint32_t>(rendererResources->scene->getExternalLights().size());
		createShaderTable(pCurrentCommandList);
	}

	if ((changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::Camera)) && rendererResources->camera->hasSensorChanged())
	{
		generateIrrToRadTexture(pCurrentCommandList, rendererResources->getTempResource());
		createShaderResources(pCurrentCommandList);
	}

	// Process components
	for (auto& ltShader : ltShaders)
		if (ltShader.component)
			ltShader.component->onChange(pCurrentCommandList, currentBackBufferIndex, changeEvent);
	
	clear = true;
	clearCaustics = (changeEvent & ~static_cast<ChangeEvent_t>(ChangeEvent::Clear)) || allowClearCaustics;
}

void LightTracingShading::onResize()
{
	auto commandList = rendererResources->commandQueue->getCommandList();
	DXResource irrToRadTempBuffer;
	generateIrrToRadTexture(commandList, irrToRadTempBuffer);
	guassianCS.bindResources();
	guassianCS.setInputTexture(static_cast<uint32_t>(guassResources.size() - 1));

	// Process components
	for (auto& ltShader : ltShaders)
		if (ltShader.component)
			ltShader.component->onResize();
	createShaderResources(commandList);
	clear = clearCaustics = true;

	// Wait
	auto fV = rendererResources->commandQueue->executeCommandList(commandList);
	rendererResources->commandQueue->waitForFenceValue(fV);
}

void LightTracingShading::accept(IVisitor* visitor)
{
	visitor->visit(this);
}

uint32_t LightTracingShading::getBufferUsage() const
{
	auto &c = ltShaders[currentShader].component;
	auto ret = c ? c->getBufferUsage() : BufferUsage::Diffuse; //
	if (constBuffer.seperateCaustics)
		ret |= BufferUsage::Caustics;
	return ret;
}

const UVector2& LightTracingShading::getLightSamples() const
{
	return lightSamples;
}

void LightTracingShading::setLightSamples(const UVector2& p_lightSamples)
{
	lightSamples = p_lightSamples;
}

void LightTracingShading::setCurrentShaderIndex(uint32_t currentShaderIndex)
{
	currentShader = currentShaderIndex;
}

uint32_t LightTracingShading::getCurrentShaderIndex() const
{
	return currentShader;
}

uint32_t LightTracingShading::getPathFilter() const
{
	return constBuffer.pathFilter;
}

void LightTracingShading::setPathFilter(uint32_t pathFilter)
{
	constBuffer.pathFilter = pathFilter;
}

bool LightTracingShading::getAllowClearCaustics() const
{
	return allowClearCaustics;
}

void LightTracingShading::setAllowClearCautics(bool p_allowClearCaustics)
{
	allowClearCaustics = p_allowClearCaustics;
}

void LightTracingShading::setCausticsBlurSize(std::uint32_t cSize)
{
	guassianCS.setFiltersize(cSize);
}

uint32_t LightTracingShading::getCausticsBlurSize() const
{
	return guassianCS.getFiltersize();
}

uint32_t LightTracingShading::getMinBounces() const
{
	return constBuffer.minBounces;
}

void LightTracingShading::setMinBounces(uint32_t minBounces)
{
	constBuffer.minBounces = minBounces;
}

uint32_t LightTracingShading::getMaxBounces() const
{
	return constBuffer.maxBounces;
}

void LightTracingShading::setMaxBounces(uint32_t maxBounces)
{
	constBuffer.maxBounces = maxBounces;
}

uint32_t LightTracingShading::getSeperateCaustics() const
{
	return constBuffer.seperateCaustics;
}

void LightTracingShading::seperateCaustics(std::uint32_t sepCaustics)
{
	constBuffer.seperateCaustics = sepCaustics;
}

uint32_t LightTracingShading::getRangeBits() const
{
	return constBuffer.rangeBits;
}

void LightTracingShading::setRangeBits(uint32_t rangeBits)
{
	constBuffer.rangeBits = rangeBits;
}

// Compute constants
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

	return candela::mathematics::f1Definite(pt0.x, pt1.x, pt0.y, pt1.y, rendererResources->camera->getNearPlaneDimensions().m128_f32[2]);
}