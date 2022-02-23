#include "LightTracingShading.h"

#include <DirectXMath.h>
#include <d3dcompiler.h>

#include "DirectX/d3dx12.h"

#include "DirectX/DxUtil.h"

#include "Exception/WindowException.h"

using std::uint32_t;
using std::vector;
using std::string;
using std::unordered_map;
using std::unique_ptr;
using std::make_unique;
using std::make_shared;

using DirectX::XMFLOAT3X4;

using Microsoft::WRL::ComPtr;

using candela::directx::DXUtil;
using candela::directx::RootSignatureManager;
using candela::directx::ShadingTable;
using candela::directx::ShadingRecordType;

using candela::renderer::LightTracingShading;
using candela::renderer::RendererResources;

using candela::mathematics::UVector2;
using candela::mathematics::Vector2;
using candela::mathematics::Vector3;

using candela::sampler::ISampler;

LightTracingShading::LightTracingShading(unique_ptr<ISampler> sampler)
	: rendererResources(), constBuffer(), sampler(std::move(sampler)), clear()
{
}

void LightTracingShading::init(RendererResources* rRes)
{
	rendererResources = rRes;

	constantTempBuffer.resize(rRes->numBackBuffers);
	tlasTempBuffer.resize(rRes->numBackBuffers);
	constBuffer.numLights = static_cast<uint32_t>(rRes->scene->getLights().size());

	auto commandList = rRes->commandQueue->getCommandList();
	auto& scene = *rRes->scene;

	const DXUtil::BottomLevelAccelerationData blasReferenceData
	{
		.vertexBuffer = rRes->sceneBuffer->GetGPUVirtualAddress(),
		.indexBuffer = rRes->sceneBuffer->GetGPUVirtualAddress() + scene.getIndicesOffset(),
		.vertexCount = static_cast<UINT>(scene.getVertices().size()),
		.indexCount = static_cast<UINT>(scene.getIndices().size()),
	};

	// Build bottom-layer - This incorporates all meshes - one BLAS per group
	unordered_map<string, size_t> bufferMap;
	for (auto &item : scene.getMeshIndexedSpanDataMap())
	{
		auto mis = &item.second;
		
		DXUtil::BottomLevelAccelerationData blasData = blasReferenceData;
		blasData.indexBuffer += static_cast<UINT>(mis->Start) * sizeof(int);
		blasData.indexCount = static_cast<UINT>(mis->Size);

		bufferMap[item.first] = blasBuffers.size();
		blasBuffers.push_back(DXUtil::createBottomLevelAS(rRes->pDevice, commandList, { blasData }, 3 * sizeof(float)));
	}

	for (const auto &child : scene.getSceneGraph().Children)
	{
		auto &indexedSpan = scene.getMeshIndexedSpan(child.GroupName);
		auto &ref = tlasInstanceData.emplace_back(DXUtil::TopLevelAccelerationData {
			.instanceId = indexedSpan.Start,
			.blasBuffer = blasBuffers[bufferMap.at(child.GroupName)]
		});
		XMStoreFloat3x4(&ref.transform, child.Transform);
	}

	// Build Top-Layer
	wrl::ComPtr<ID3D12Resource> tlasTempBuffer;
	DXUtil::buildTopLevelAS(rRes->pDevice, commandList, tlasInstanceData, tlasTempBuffer, false, tlasBuffers);

	// Compute irradianceToRadianceConstants
	wrl::ComPtr<ID3D12Resource> irrToRadTempBuffer;
	vector<float> irradianceToRadianceConstants;
	irradianceToRadianceConstants.reserve(rRes->winDimensions.x * rRes->winDimensions.y);
	for (uint32_t y = 0; y < rRes->winDimensions.y; ++y)
		for (uint32_t x = 0; x < rRes->winDimensions.x; ++x)
			irradianceToRadianceConstants.push_back(1.f / cosIntegral(x, y));
	irrToRad = DXUtil::uploadTextureDataToDefaultHeap(rendererResources->pDevice, commandList, irrToRadTempBuffer, irradianceToRadianceConstants.data(),
		rRes->winDimensions.x, rRes->winDimensions.y, sizeof(float), DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Build Pipeline
	buildPipeline();

	// Create Shader resources
	createShaderResources();

	// Constant buffer
	wrl::ComPtr<ID3D12Resource> cBuffIntBuffer;
	constantBuffer = DXUtil::uploadDataToDefaultHeap(rRes->pDevice, commandList, cBuffIntBuffer, &constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Build shading table
	wrl::ComPtr<ID3D12Resource> stTempBuffer;
	createShaderTable(commandList, stTempBuffer);

	// Wait
	auto fV = rRes->commandQueue->executeCommandList(commandList);
	rRes->commandQueue->waitForFenceValue(fV);
}

void LightTracingShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList, uint32_t currentBackBufferIndex)
{
	// Pre-stuff
	auto &backBuff = rendererResources->pRTVBackBuffers[currentBackBufferIndex];
	auto b1 = CD3DX12_RESOURCE_BARRIER::Transition(backBuff.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	currentCommandList->ResourceBarrier(1u, &b1);

	currentCommandList->SetDescriptorHeaps(1u, descriptorHeap.GetAddressOf());
	auto t1 = CD3DX12_RESOURCE_BARRIER::Transition(outputTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	currentCommandList->ResourceBarrier(1u, &t1);

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
	clear |= cam->hasChanged();
	constBuffer.clear = clear ? 1 : 0;
	clear = false;
	DXUtil::updateDataInDefaultHeap(rendererResources->pDevice, currentCommandList, constantBuffer, constantTempBuffer[currentBackBufferIndex],
		&constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	currentCommandList->SetComputeRootSignature(globalEmptyRootSignature.Get());
	HRESULT hr;
	ComPtr<ID3D12GraphicsCommandList4> commandList4;
	GFXTHROWIFFAILED(currentCommandList.As(&commandList4));
	commandList4->SetPipelineState1(stateObject.Get());

	// Launch rays
	auto& dim = rendererResources->winDimensions;
	D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = shadingTable->getDispatchRaysDescriptor(dim.x, dim.y);
	commandList4->DispatchRays(&dispatchRaysDesc);

	// After
	auto t2 = CD3DX12_RESOURCE_BARRIER::Transition(outputTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	currentCommandList->ResourceBarrier(1u, &t2);
	currentCommandList->CopyResource(backBuff.Get(), outputTexture.Get());
	auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(backBuff.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
	currentCommandList->ResourceBarrier(1u, &b2);
}

void LightTracingShading::buildPipeline()
{
	rootSignatureManager = make_shared<RootSignatureManager>();
	shadingTable = make_unique<ShadingTable>(rootSignatureManager);

	HRESULT hr;

	// Define State Object Descriptor (EXTENDED version from d3dx)
	CD3DX12_STATE_OBJECT_DESC stateObjectDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	// Construct sub objects

	// First is DXIL - to load shader and Load symbols from the shader and identify the entry points
	wrl::ComPtr<ID3DBlob> pBlob;
	GFXTHROWIFFAILED(D3DReadFileToBlob(L"./Shaders/LightTracingShader.cso", &pBlob));
	CD3DX12_DXIL_LIBRARY_SUBOBJECT dxilSubObject(stateObjectDesc);
	auto shaderByteCodeDesc = CD3DX12_SHADER_BYTECODE(pBlob.Get());
	dxilSubObject.SetDXILLibrary(&shaderByteCodeDesc);
	const WCHAR* entryPoints[] = { L"rayGen", L"miss", L"chs", L"shadowMiss" };
	dxilSubObject.DefineExports(entryPoints);

	// Second - Hit Program - link to entry point names
	CD3DX12_HIT_GROUP_SUBOBJECT hitSubObject(stateObjectDesc);
	hitSubObject.SetClosestHitShaderImport(L"chs");
	hitSubObject.SetHitGroupExport(L"HitGroup");

	// Third - Local Root Signature for Ray Gen shader
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0)); //gOutput, gIrradiance
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 8)); //gRtScene, gIrrToRad
	if (!rendererResources->textures.empty())
		rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(rendererResources->textures.size()), 10));
	
	rootSignatureManager->setDescriptorTableParameter("BVHDescTable", "BVH");
	CD3DX12_ROOT_PARAMETER1 param;
	param.InitAsConstantBufferView(0);
	rootSignatureManager->setParameter("ConstBuff", param);

	param.InitAsShaderResourceView(0); rootSignatureManager->setParameter("verts", param);
	param.InitAsShaderResourceView(1); rootSignatureManager->setParameter("texVerts", param);
	param.InitAsShaderResourceView(2); rootSignatureManager->setParameter("normals", param);
	param.InitAsShaderResourceView(3); rootSignatureManager->setParameter("indices", param);
	param.InitAsShaderResourceView(4); rootSignatureManager->setParameter("matrices", param);
	param.InitAsShaderResourceView(5); rootSignatureManager->setParameter("faceAttributes", param);
	param.InitAsShaderResourceView(6); rootSignatureManager->setParameter("materials", param);
	param.InitAsShaderResourceView(7); rootSignatureManager->setParameter("lights", param);

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

	rootSignatureManager->addParametersToRootSignature("RayGenRootSignature", { "BVHDescTable", "ConstBuff", "verts", "texVerts", "normals", "indices", "matrices", "faceAttributes", "materials", "lights" });
	rootSignatureManager->setSamplerForRootSignature("RayGenRootSignature", sampler);
	rootSignatureManager->generateRootSignature("RayGenRootSignature", rendererResources->pDevice);

	shadingTable->addProgram(L"rayGen", ShadingRecordType::RayGeneration, "RayGenRootSignature");

	// Fifth - create empty lrs for miss program
	rootSignatureManager->addRootSignature("EmptyRootSignature");
	rootSignatureManager->generateRootSignature("EmptyRootSignature", rendererResources->pDevice);

	// Hit Group Signature
	rootSignatureManager->addParametersToRootSignature("HitGroupSignature", { "BVHDescTable", "ConstBuff", "verts", "texVerts", "normals", "indices", "matrices", "faceAttributes", "materials", "lights"});
	rootSignatureManager->setSamplerForRootSignature("HitGroupSignature", sampler);
	rootSignatureManager->generateRootSignature("HitGroupSignature", rendererResources->pDevice);

	// Sixth - Associate the empty local root signature with the miss programs
	shadingTable->addProgram(L"miss", ShadingRecordType::Miss, "EmptyRootSignature");
	shadingTable->addProgram(L"shadowMiss", ShadingRecordType::Miss, "EmptyRootSignature");
	shadingTable->addProgram(L"HitGroup", ShadingRecordType::HitGroup, "HitGroupSignature");

	// Generate/add subobjects
	rootSignatureManager->addRootSignaturesToSubObject(stateObjectDesc);
	shadingTable->addProgramAssociationsToSubobject(stateObjectDesc);

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
	rtPipelineConfig.Config(2);

	// Tenth - Global Root Signature
	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT globalRootSignature(stateObjectDesc);
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC globalEmptyRootSignatureDesc(0, static_cast<CD3DX12_ROOT_PARAMETER1*>(nullptr), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
	globalEmptyRootSignature = DXUtil::createRootSignature(rendererResources->pDevice, globalEmptyRootSignatureDesc);
	globalRootSignature.SetRootSignature(globalEmptyRootSignature.Get());

	// Finally - Create the state
	ComPtr<ID3D12Device5> pDevice5;
	GFXTHROWIFFAILED(rendererResources->pDevice.As(&pDevice5));
	GFXTHROWIFFAILED(pDevice5->CreateStateObject(stateObjectDesc, IID_PPV_ARGS(&stateObject)));
}

void LightTracingShading::createShaderResources()
{
	size_t entryNumber = 0;
	// The descriptor heap to store SRV (Shader resource View) and UAV (Unordered access view) descriptors
	auto& descHeapManager = shadingTable->generateDescriptorHeap("BVHDescTable", "BVH1", rendererResources->pDevice);
	descriptorHeap = descHeapManager.getDescriptorHeap();

	const auto &dim = rendererResources->winDimensions;
	// The output resource
	outputTexture = DXUtil::createTextureCommittedResource(rendererResources->pDevice, D3D12_HEAP_TYPE_DEFAULT, dim.x, dim.y, D3D12_RESOURCE_STATE_COPY_SOURCE);
	irradianceTexture = DXUtil::createTextureCommittedResource(rendererResources->pDevice, D3D12_HEAP_TYPE_DEFAULT, dim.x, dim.y, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_NONE, DXGI_FORMAT_R32G32B32A32_FLOAT);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	descHeapManager.setUAV(entryNumber++, uavDesc, rendererResources->pDevice, outputTexture);
	descHeapManager.setUAV(entryNumber++, uavDesc, rendererResources->pDevice, irradianceTexture);

	// Create the SRV descriptor in second place (following same order as in root signature)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = tlasBuffers.pResult->GetGPUVirtualAddress();
	descHeapManager.setSRV(entryNumber++, srvDesc, rendererResources->pDevice);

	// Irr to Rad?
	srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	descHeapManager.setSRV(entryNumber++, srvDesc, rendererResources->pDevice, irrToRad);

	// Textures
	srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	for (const auto& texture : rendererResources->textures)
		descHeapManager.setSRV(entryNumber++, srvDesc, rendererResources->pDevice, texture);
}

void LightTracingShading::createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList> &commandList, wrl::ComPtr<ID3D12Resource> &tempResource)
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
	shadingTable->setInputForViewParameter(L"rayGen", "faceAttributes", rendererResources->faceAttributeBuffer);
	shadingTable->setInputForViewParameter(L"rayGen", "materials", rendererResources->materialBuffer);
	shadingTable->setInputForViewParameter(L"rayGen", "lights", rendererResources->lightBuffer);

	// Link HitGroup - Bindings for 'chs'
	shadingTable->setInputForDescriptorTableParameter(L"HitGroup", "BVHDescTable", "BVH1");
	shadingTable->setInputForViewParameter(L"HitGroup", "ConstBuff", constantBuffer);
	shadingTable->setInputForViewParameter(L"HitGroup", "verts", rendererResources->sceneBuffer, rendererResources->scene->getVerticesOffset());
	shadingTable->setInputForViewParameter(L"HitGroup", "texVerts", rendererResources->sceneBuffer, rendererResources->scene->getTextureCoordsOffset());
	shadingTable->setInputForViewParameter(L"HitGroup", "normals", rendererResources->sceneBuffer, rendererResources->scene->getNormalsOffset());
	shadingTable->setInputForViewParameter(L"HitGroup", "indices", rendererResources->sceneBuffer, rendererResources->scene->getIndicesOffset());
	shadingTable->setInputForViewParameter(L"HitGroup", "matrices", rendererResources->matrices);
	shadingTable->setInputForViewParameter(L"HitGroup", "faceAttributes", rendererResources->faceAttributeBuffer);
	shadingTable->setInputForViewParameter(L"HitGroup", "materials", rendererResources->materialBuffer);
	shadingTable->setInputForViewParameter(L"HitGroup", "lights", rendererResources->lightBuffer);

	// Generate
	shadingTable->generateShadingTable(rendererResources->pDevice, commandList, stateObject, tempResource);
}

void LightTracingShading::buildTlas(wrl::ComPtr<ID3D12GraphicsCommandList> &commandList, wrl::ComPtr<ID3D12Resource>& tempResource)
{
	// Warning, we are assuming order - will not be the case when instancing in the future
	auto tlas = tlasInstanceData.begin();
	for (const auto &child : rendererResources->scene->getSceneGraph().Children)
		XMStoreFloat3x4(&(tlas++)->transform, child.Transform);

	DXUtil::buildTopLevelAS(rendererResources->pDevice, commandList, tlasInstanceData, tempResource, true, tlasBuffers);
}

void LightTracingShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex)
{
	buildTlas(pCurrentCommandList, tlasTempBuffer[currentBackBufferIndex]);
	clear = true;
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