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
using std::make_unique;
using std::make_shared;

using DirectX::XMFLOAT3X4;

using candela::directx::DXUtil;
using candela::directx::RootSignatureManager;
using candela::directx::ShadingTable;
using candela::directx::ShadingRecordType;

using candela::renderer::LightTracingShading;
using candela::renderer::RendererResources;

LightTracingShading::LightTracingShading()
	: rendererResources(), constBuffer()
{
}

void LightTracingShading::init(RendererResources* rRes)
{
	rendererResources = rRes;

	auto commandList = rRes->commandQueue->getCommandList();
	//vector<D3D12_GPU_VIRTUAL_ADDRESS>& vertexBuffers;
	auto& scene = *rRes->scene;

	// Build bottom-layer - This incorporates all meshes
	unordered_map<string, size_t> bufferMap;
	for (auto &item : scene.getMeshIndexedSpanDataMap())
	{
		auto mis = &item.second;
		bufferMap[item.first] = blasBuffers.size();
		blasBuffers.push_back(
			DXUtil::createBottomLevelAS(
				rRes->pDevice, commandList, 
				{ rRes->sceneBuffer->GetGPUVirtualAddress() + mis->Start * 3 * sizeof(float) },
				{ static_cast<uint32_t>(mis->Size) / 3 }, 3 * sizeof(float)
			)
		);
	}

	vector<DXUtil::TopLevelAccelerationData> instanceData;
	for (auto child : scene.getSceneGraph().Children)
	{
		auto &indexedSpan = scene.getMeshIndexedSpan(child.GroupName);
		auto &ref = instanceData.emplace_back(DXUtil::TopLevelAccelerationData {
			.instanceId = indexedSpan.Start,
			.blasBuffer = blasBuffers[bufferMap.at(child.GroupName)]
		});
		XMStoreFloat3x4(&ref.transform, child.Transform);
	}

	// Build Top-Layer
	wrl::ComPtr<ID3D12Resource> tlasTempBuffer;
	DXUtil::buildTopLevelAS(rRes->pDevice, commandList, instanceData, tlasTempBuffer, false, tlasBuffers);

	// Build Pipeline
	buildPipeline();

	// Create Shader resources
	createShaderResources();

	// Build shading table
	wrl::ComPtr<ID3D12Resource> stTempBuffer;
	createShaderTable(commandList, stTempBuffer);

	// Wait
	auto fV = rRes->commandQueue->executeCommandList(commandList);
	rRes->commandQueue->waitForFenceValue(fV);
}

void LightTracingShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList6> currentCommandList, uint32_t currentBackBufferIndex)
{
	// Pre-stuff
	auto &backBuff = rendererResources->pRTVBackBuffers[currentBackBufferIndex];
	auto b1 = CD3DX12_RESOURCE_BARRIER::Transition(backBuff.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	currentCommandList->ResourceBarrier(1u, &b1);

	currentCommandList->SetDescriptorHeaps(1u, descriptorHeap.GetAddressOf());
	auto t1 = CD3DX12_RESOURCE_BARRIER::Transition(outputTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	currentCommandList->ResourceBarrier(1u, &t1);

	currentCommandList->SetComputeRootSignature(globalEmptyRootSignature.Get());
	currentCommandList->SetPipelineState1(stateObject.Get());

	// Launch rays
	auto& dim = rendererResources->winDimensions;
	D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = shadingTable->getDispatchRaysDescriptor(dim.x, dim.y);

	currentCommandList->DispatchRays(&dispatchRaysDesc);

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
	const WCHAR* entryPoints[] = { L"rayGen", L"miss", L"chs", L"shadowChs" };
	dxilSubObject.DefineExports(entryPoints);

	// Second - Hit Program - link to entry point names
	CD3DX12_HIT_GROUP_SUBOBJECT hitSubObject(stateObjectDesc);
	hitSubObject.SetClosestHitShaderImport(L"chs");
	hitSubObject.SetHitGroupExport(L"HitGroup");

	CD3DX12_HIT_GROUP_SUBOBJECT shadowHitSubObject(stateObjectDesc);
	shadowHitSubObject.SetClosestHitShaderImport(L"shadowChs");
	shadowHitSubObject.SetHitGroupExport(L"ShadowHitGroup");

	// Third - Local Root Signature for Ray Gen shader
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE)); //gOutput
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE)); //gRtScene
	rootSignatureManager->setDescriptorTableParameter("BVHDescTable", "BVH");

	rootSignatureManager->addParametersToRootSignature("RayGenRootSignature", { "BVHDescTable" });
	rootSignatureManager->generateRootSignature("RayGenRootSignature", rendererResources->pDevice);

	shadingTable->addProgram(L"rayGen", ShadingRecordType::RayGeneration, "RayGenRootSignature");

	// Fifth - create empty lrs for miss program
	rootSignatureManager->addRootSignature("EmptyRootSignature");
	rootSignatureManager->generateRootSignature("EmptyRootSignature", rendererResources->pDevice);

	// Sixth - Associate the empty local root signature with the miss programs
	shadingTable->addProgram(L"miss", ShadingRecordType::Miss, "EmptyRootSignature");
	shadingTable->addProgram(L"HitGroup", ShadingRecordType::HitGroup, "EmptyRootSignature");
	shadingTable->addProgram(L"ShadowHitGroup", ShadingRecordType::HitGroup, "EmptyRootSignature");

	// Generate/add subobjects
	rootSignatureManager->addRootSignaturesToSubObject(stateObjectDesc);
	shadingTable->addProgramAssociationsToSubobject(stateObjectDesc);

	// Seventh - Shader Configuration (set payload sizes - the actual program parameters)
	CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT shaderConfig(stateObjectDesc);
	shaderConfig.Config(4 * sizeof(float), 2 * sizeof(float));

	// Eighth - Associate the shader configuration with all shader programs
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT shaderConfigAssociation(stateObjectDesc);
	shaderConfigAssociation.AddExport(L"rayGen");
	shaderConfigAssociation.AddExport(L"miss");
	shaderConfigAssociation.AddExport(L"HitGroup");
	shaderConfigAssociation.AddExport(L"ShadowHitGroup");
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
	GFXTHROWIFFAILED(rendererResources->pDevice->CreateStateObject(stateObjectDesc, IID_PPV_ARGS(&stateObject)));
}

void LightTracingShading::createShaderResources()
{
	// The descriptor heap to store SRV (Shader resource View) and UAV (Unordered access view) descriptors
	auto& descHeapManager = shadingTable->generateDescriptorHeap("BVHDescTable", "BVH1", rendererResources->pDevice);
	descriptorHeap = descHeapManager.getDescriptorHeap();

	const auto &dim = rendererResources->winDimensions;
	// The output resource
	outputTexture = DXUtil::createTextureCommittedResource(rendererResources->pDevice, D3D12_HEAP_TYPE_DEFAULT, dim.x, dim.y, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	descHeapManager.setUAV(0, uavDesc, rendererResources->pDevice, outputTexture);

	// Create the SRV descriptor in second place (following same order as in root signature)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = tlasBuffers.pResult->GetGPUVirtualAddress();
	descHeapManager.setSRV(1, srvDesc, rendererResources->pDevice);

}

void LightTracingShading::createShaderTable(wrl::ComPtr<ID3D12GraphicsCommandList6> &commandList, wrl::ComPtr<ID3D12Resource> &tempResource)
{
	// Link elements
	shadingTable->setInputForDescriptorTableParameter(L"rayGen", "BVHDescTable", "BVH1");
	//shadingTable->setInputForDescriptorTableParameter(L"HitGroup", "BVHDescTable", "BVH1");
	shadingTable->generateShadingTable(rendererResources->pDevice, commandList, stateObject, tempResource);
}
