#include "RayTracingAOShading.h"
#include "AccelerationStructure.h"

#include "Exception/WindowException.h"

#include "Util/StringUtil.h"
#include <d3dcompiler.h>


using std::uint32_t;
using std::unique_ptr;
using std::make_unique;
using std::make_shared;

using Microsoft::WRL::ComPtr;

using candela::renderer::RayTracingAOShading;
using candela::renderer::AccelerationStructure;
using candela::renderer::RendererResources;
using candela::renderer::ResourceRegFunction;

using candela::directx::DXUtil;
using candela::directx::DXCommandList;
using candela::directx::RootSignatureManager;
using candela::directx::DescriptorHeap;
using candela::directx::ShadingTable;
using candela::directx::ShadingRecordType;
using candela::directx::Resource;

using candela::util::StringToWString;


RayTracingAOShading::RayTracingAOShading(std::vector<std::string> inputs, std::vector<std::string> outputs)
	: rendererResources(), cdfMask(), constBuffer{}, inputs(std::move(inputs)), outputs(std::move(outputs))
{
}

void RayTracingAOShading::init(RendererResources* rRes, DXCommandList& pCurrentCommandList, ResourceRegFunction& resRegFn)
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

	for (const auto& resName : inputs)
		inputResources.push_back(rendererResources->resourceManager->getNamedResource(resName));

	auto dim = rRes->resourceManager->getNamedResource(inputs.at(0))->getDimensions();

	cdfMask = &rRes->resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		dim.x, dim.y, DXGI_FORMAT_R32_FLOAT, false, outputs.at(0));

	buildPipeline();
	createDescriptorTable();
	createShaderTable(pCurrentCommandList);
}

void RayTracingAOShading::draw(DXCommandList pCurrentCommandList, std::uint32_t currentBackBufferIndex)
{
	for (auto res : inputResources)
		res->transitionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	cdfMask->uavBarrier(pCurrentCommandList);

	pCurrentCommandList->SetDescriptorHeaps(1u, descHeapManager->getDescriptorHeap().GetAddressOf());
	pCurrentCommandList->SetComputeRootSignature(globalRootSignature.Get());
	pCurrentCommandList->SetComputeRoot32BitConstants(0u, 3u, &constBuffer, 0u);

	HRESULT hr;
	ComPtr<ID3D12GraphicsCommandList4> commandList4;
	GFXTHROWIFFAILED(pCurrentCommandList.As(&commandList4));
	commandList4->SetPipelineState1(stateObject.Get());

	// Launch rays
	auto rayDimensions = cdfMask->getDimensions();
	D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = shadingTable->getDispatchRaysDescriptor(rayDimensions.x, rayDimensions.y);
	commandList4->DispatchRays(&dispatchRaysDesc);

	for (auto res : inputResources)
		res->transitionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void RayTracingAOShading::onChange(DXCommandList pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
}

void RayTracingAOShading::onResize()
{
}

void RayTracingAOShading::accept(IVisitor* visitor)
{
}

void RayTracingAOShading::setCameraPosition(const DirectX::XMVECTOR& cameraPos)
{
	constBuffer.cameraPos = cameraPos;
}

void RayTracingAOShading::buildPipeline()
{
	rootSignatureManager = make_shared<RootSignatureManager>();

	HRESULT hr;

	// Define State Object Descriptor (EXTENDED version from d3dx)
	CD3DX12_STATE_OBJECT_DESC stateObjectDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	// First is DXIL - to load shader and Load symbols from the shader and identify the entry points
	CD3DX12_DXIL_LIBRARY_SUBOBJECT dxilSubObject(stateObjectDesc);
	const WCHAR* entryPoints[] = { L"rayGen", L"shadowMiss" };
	dxilSubObject.DefineExports(entryPoints);

	// Third - Local Root Signature for Ray Gen shader
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0)); // gCdfMask
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0)); //gRtScene, gBuffer stuff
	rootSignatureManager->setDescriptorTableParameter("BVHDescTable", "BVH");
	rootSignatureManager->addParametersToRootSignature("RayGenRootSignature", { "BVHDescTable" });
	rootSignatureManager->generateRootSignature("RayGenRootSignature", rendererResources->pDevice);
	rootSignatureManager->addRootSignature("EmptyRootSignature");
	rootSignatureManager->generateRootSignature("EmptyRootSignature", rendererResources->pDevice);
	rootSignatureManager->addRootSignaturesToSubObject(stateObjectDesc);

	// Seventh - Shader Configuration (set payload sizes - the actual program parameters)
	CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT shaderConfig(stateObjectDesc);
	shaderConfig.Config(5 * sizeof(float), 2 * sizeof(float));

	// Eighth - Associate the shader configuration with all shader programs
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT shaderConfigAssociation(stateObjectDesc);
	LPCWSTR exports[] = { L"rayGen", L"shadowMiss" };
	shaderConfigAssociation.AddExports(exports);
	shaderConfigAssociation.SetSubobjectToAssociate(shaderConfig);

	// Ninth - Configure the RAY TRACING PIPELINE
	CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT rtPipelineConfig(stateObjectDesc);
	rtPipelineConfig.Config(1);

	// Tenth - Global Root Signature
	auto rsm = std::make_shared<RootSignatureManager>();
	CD3DX12_ROOT_PARAMETER1 param; param.InitAsConstants(3u, 0);
	rsm->setParameter("cameraPos", param);
	rsm->addParametersToRootSignature("ComputeRootSignature", { "cameraPos" });
	globalRootSignature = rsm->generateRootSignature("ComputeRootSignature", rendererResources->pDevice, D3D12_ROOT_SIGNATURE_FLAG_NONE);
	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT globalRootSignatureSubObj(stateObjectDesc);
	globalRootSignatureSubObj.SetRootSignature(globalRootSignature.Get());

	// Generate shading table and add required associations to stateobject
	shadingTable = make_unique<ShadingTable>(rootSignatureManager);
	shadingTable->addProgram(L"rayGen", ShadingRecordType::RayGeneration, "RayGenRootSignature");
	shadingTable->addProgram(L"shadowMiss", ShadingRecordType::Miss, "EmptyRootSignature");
	shadingTable->addProgramAssociationsToSubobject(stateObjectDesc);

	// Load shader
	wrl::ComPtr<ID3DBlob> pBlob = DXUtil::LoadShaderResource("./Shaders/RayTracingAOShader.cso");
	auto shaderByteCodeDesc = CD3DX12_SHADER_BYTECODE(pBlob.Get());
	dxilSubObject.SetDXILLibrary(&shaderByteCodeDesc);

	// Create state object with all this configuration
	ComPtr<ID3D12Device5> pDevice5;
	GFXTHROWIFFAILED(rendererResources->pDevice.As(&pDevice5));
	GFXTHROWIFFAILED(pDevice5->CreateStateObject(stateObjectDesc, IID_PPV_ARGS(&stateObject)));
}

void RayTracingAOShading::createDescriptorTable()
{
	size_t entryNumber = 0;

	// The descriptor heap to store SRV (Shader resource View) and UAV (Unordered access view) descriptors
	if (!descHeapManager)
	{
		descHeapManager = make_shared<DescriptorHeap>(rootSignatureManager, "BVHDescTable", "BVH1", rendererResources->pDevice);
		shadingTable->setDescriptorHeapIfNotExists(descHeapManager);
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
	descHeapManager->setUAV(entryNumber++, uavDesc, rendererResources->pDevice, *cdfMask);

	// Create the SRV descriptor in second place (following same order as in root signature)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = rendererResources->accelerationStructure->getTopLayerBufferAddress();
	descHeapManager->setSRV(entryNumber++, srvDesc, rendererResources->pDevice);

	// Textures
	srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	// gBuffer
	for (auto res : inputResources)
		descHeapManager->setSRV(entryNumber++, srvDesc, rendererResources->pDevice, *res);
}

void RayTracingAOShading::createShaderTable(DXCommandList& commandList)
{
	// Link rayGen
	shadingTable->setInputForDescriptorTableParameter(L"rayGen", "BVHDescTable", "BVH1");
	shadingTable->generateShadingTable(rendererResources->pDevice, commandList, stateObject, rendererResources->getTempResource());
}
