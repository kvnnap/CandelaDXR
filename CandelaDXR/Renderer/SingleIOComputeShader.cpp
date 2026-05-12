#include <array>
#include <d3dcompiler.h>

#include "DirectX/RootSignatureManager.h"
#include "Exception/WindowException.h"
#include "Util/StringUtil.h"

#include "SingleIOComputeShader.h"

#include "Mathematics/Utils.h"

#include "DirectX/DxUtil.h"

using std::string;
using std::uint32_t;

using candela::mathematics::UVector2;
using candela::mathematics::GaussIntegral;
using candela::directx::DXUtil;
using candela::directx::DXCommandList;
using candela::directx::DescriptorHeap;
using candela::directx::RootSignatureManager;
using candela::renderer::SingleIOComputeShader;
using candela::renderer::FilterComputeShader;
using candela::renderer::DistanceComputeShader;

SingleIOComputeShader::SingleIOComputeShader(const string& shaderPath, bool launchAsFlatArray)
	: rendererResources(), shaderPath(shaderPath), launchAsFlatArray(launchAsFlatArray),
	  numInputs(), numOutputs(), resourceManager(), resources(), inputTextureIndex(), outputTextureIndex(),
	  inputTexture(), outputTexture(), pDevice(), cbData(), cbSize()
{
}

void SingleIOComputeShader::init(RendererResources* rendererResources, DXCommandList& pCurrentCommandList, std::vector<directx::Resource*>* res, uint32_t numOutputs, uint32_t numInputs)
{
	this->rendererResources = rendererResources;
	resourceManager = rendererResources->resourceManager.get();
	pDevice = rendererResources->pDevice.Get();
	resources = res;
	this->numInputs = numInputs;
	this->numOutputs = numOutputs;
	if (numInputs == 0 || numOutputs == 0)
		ThrowException("numInputs/numOutputs cannot be zero. Maybe no lights?");

	// First need to generate Root Signature
	auto rsm = std::make_shared<RootSignatureManager>();
	CD3DX12_ROOT_PARAMETER1 param;
	rsm->addDescriptorRange("IORange", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numInputs, 0u));
	rsm->addDescriptorRange("IORange", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, numOutputs, 0u));
	addAdditionalResources(rsm.get(), "IORange");
	rsm->setDescriptorTableParameter("IODescTable", "IORange");
	param.InitAsConstants(4u, 0u); rsm->setParameter("Constants", param);
	param.InitAsConstants(static_cast<UINT>(cbSize / 4), 1u); rsm->setParameter("Constants1", param);
	rsm->addParametersToRootSignature("ComputeRootSignature", { "Constants", "Constants1", "IODescTable" });
	computeRootSignature = rsm->generateRootSignature("ComputeRootSignature", pDevice, D3D12_ROOT_SIGNATURE_FLAG_NONE);
	
	// Create descriptor heap
	descHeapManager = std::make_unique<DescriptorHeap>(rsm, "IODescTable", "IO1", pDevice);

	bindResources();

	// Load Shader
	HRESULT hr;
	wrl::ComPtr<ID3DBlob> pComputeBlob = DXUtil::LoadShaderResource(shaderPath.c_str());

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

	initComponent(rendererResources, pCurrentCommandList);
}

void SingleIOComputeShader::compute(DXCommandList currentCommandList)
{
	inputTexture->getAspectRatio();
	auto dim = inputTexture->getDimensions();

	ID3D12Resource* inputTextureResource = *inputTexture;
	ID3D12Resource* outputTextureResource = *outputTexture;
	if (outputTexture->getState() != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		ThrowException("Output Texture not in UAV state");

	updateData(currentCommandList);

	std::array<UINT, 4> constants = { dim.x, dim.y, inputTextureIndex - numOutputs, outputTextureIndex };
	auto prevState = inputTexture->getState();
	inputTexture->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	currentCommandList->SetComputeRootSignature(computeRootSignature.Get());
	currentCommandList->SetPipelineState(computePipelineState.Get());
	currentCommandList->SetDescriptorHeaps(1u, descHeapManager->getDescriptorHeap().GetAddressOf());
	currentCommandList->SetComputeRoot32BitConstants(0u, static_cast<UINT>(constants.size()), constants.data(), 0u);
	if (cbData)
		currentCommandList->SetComputeRoot32BitConstants(1u, static_cast<UINT>(cbSize / 4u), cbData, 0u);
	currentCommandList->SetComputeRootDescriptorTable(2u, descHeapManager->getDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	dispatch(currentCommandList);
	inputTexture->transistionBarrier(currentCommandList, prevState);
	outputTexture->uavBarrier(currentCommandList);
}

void SingleIOComputeShader::setInputTexture(uint32_t inputIndex)
{
	inputTextureIndex = inputIndex;
	inputTexture = (*resources)[inputTextureIndex];

	bindAdditionalResources(numInputs + numOutputs);
}

void SingleIOComputeShader::setOutputTexture(uint32_t outputIndex)
{
	outputTextureIndex = outputIndex;
	outputTexture = (*resources)[outputTextureIndex];
}

void SingleIOComputeShader::setAdditionalConstantBuffer(const void* p_cbData, std::size_t p_cbSize)
{
	cbData = p_cbData;
	cbSize = p_cbSize;
}

UVector2 SingleIOComputeShader::getLaunchDimensions(const UVector2& dim) const
{
	if (launchAsFlatArray)
	{
		const auto totalSize = dim.x * dim.y;
		return UVector2(totalSize / (ThreadGroupDim * ThreadGroupDim) + (totalSize % (ThreadGroupDim * ThreadGroupDim) == 0u ? 0u : 1u), 1u);
	}
	else
	{
		return UVector2(dim.x / ThreadGroupDim + (dim.x % ThreadGroupDim == 0u ? 0u : 1u), dim.y / ThreadGroupDim + (dim.y % ThreadGroupDim == 0u ? 0u : 1u));
	}
}

void SingleIOComputeShader::addAdditionalResources(RootSignatureManager* rsm, const std::string& rangeName)
{
}

void SingleIOComputeShader::bindAdditionalResources(UINT baseIndex)
{
}

void SingleIOComputeShader::updateData(DXCommandList currentCommandList)
{
}

void SingleIOComputeShader::dispatch(DXCommandList currentCommandList)
{
	auto dim = inputTexture->getDimensions();
	auto launchDimensions = getLaunchDimensions(dim);
	currentCommandList->Dispatch(launchDimensions.x, launchDimensions.y, 1u);
}

void SingleIOComputeShader::initComponent(RendererResources* rendererResources, DXCommandList& pCurrentCommandList)
{
}

void SingleIOComputeShader::bindResources()
{
	// Bind resources
	uint32_t entryNum{};
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		//srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1u;
		for (uint32_t i = numOutputs; i < numInputs + numOutputs; ++i)
			descHeapManager->setSRV(entryNum++, srvDesc, pDevice, *(*resources)[i]);
	}

	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		//uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
		for (uint32_t i = 0; i < numOutputs; ++i)
			descHeapManager->setUAV(entryNum++, uavDesc, pDevice, *(*resources)[i]);
	}
}

// Prefix sum
using candela::renderer::PrefixSumComputeShader;

PrefixSumComputeShader::PrefixSumComputeShader()
	: SingleIOComputeShader("./Shaders/PrefixSumComputeShader.cso", true), scratchResource()
{
	// Dummy resource
	setAdditionalConstantBuffer(nullptr, sizeof(std::uint32_t));
}

UVector2 PrefixSumComputeShader::getLaunchDimensions(const UVector2& dim) const
{
	auto totalSize = dim.x * dim.y;
	totalSize = totalSize / 2u + (totalSize % 2u == 0u ? 0u : 1u);
	return UVector2(totalSize / ThreadGroupDim + (totalSize % ThreadGroupDim == 0u ? 0u : 1u), 1u);
}

void PrefixSumComputeShader::addAdditionalResources(RootSignatureManager* rsm, const std::string& rangeName)
{
	rsm->addDescriptorRange(rangeName, CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1u, 0u, 1u));
}

void PrefixSumComputeShader::bindAdditionalResources(UINT baseIndex)
{
	auto dim = inputTexture->getDimensions();
	auto reqSize = getLaunchDimensions(dim).x;

	// Create resouce
	if (scratchResource)
		scratchResource->resize(reqSize * sizeof(float), 1u);
	else
		scratchResource = &resourceManager->createResource(D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, reqSize * sizeof(float));
	
	// Describe resource and add to descriptor heap
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.NumElements = reqSize;
	uavDesc.Buffer.StructureByteStride = sizeof(float);
	descHeapManager->setUAV(baseIndex, uavDesc, pDevice, *scratchResource);
}

void PrefixSumComputeShader::dispatch(DXCommandList currentCommandList)
{
	auto dim = inputTexture->getDimensions();
	auto launchDimensions = getLaunchDimensions(dim);

	// First Pass - Perform Blelloch Scan on 2048 items per block (block size 1024)
	currentCommandList->SetComputeRoot32BitConstant(1u, 0u, 0u);
	currentCommandList->Dispatch(launchDimensions.x, launchDimensions.y, 1u);
	outputTexture->uavBarrier(currentCommandList);
	if (scratchResource->getState() != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		scratchResource->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	else
		scratchResource->uavBarrier(currentCommandList);

	// If we only have one block, the operation is complete
	if (launchDimensions.x <= 1u)
		return;

	// Second Pass - Compute Scan on the summed stuff
	currentCommandList->SetComputeRoot32BitConstant(1u, 1u, 0u);
	currentCommandList->Dispatch(launchDimensions.x / ThreadGroupDim + (launchDimensions.x % ThreadGroupDim == 0u ? 0u : 1u), launchDimensions.y, 1u);
	scratchResource->uavBarrier(currentCommandList);

	// Third Pass - Add sum to blocks
	currentCommandList->SetComputeRoot32BitConstant(1u, 2u, 0u);
	currentCommandList->Dispatch(launchDimensions.x - 1u, launchDimensions.y, 1u);
	//outputTexture->uavBarrier(currentCommandList);
}

FilterComputeShader::FilterComputeShader()
	: SingleIOComputeShader("./Shaders/FilterComputeShader.cso"), cbvResource(), scratchResource(), scratchResFormat(DXGI_FORMAT_R32_FLOAT), changed()
{
	setAdditionalConstantBuffer(nullptr, sizeof(std::uint32_t) * 2); // Send gausian size and pass number
	setFiltersize(InitialSize);
}

void FilterComputeShader::addAdditionalResources(RootSignatureManager* rsm, const std::string& rangeName)
{
	rsm->addDescriptorRange(rangeName, CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1u, 0u, 1u)); // Pass linear filter data
	rsm->addDescriptorRange(rangeName, CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1u, 0u, 1u)); // Pass Scratch
}

void FilterComputeShader::bindAdditionalResources(UINT baseIndex)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.NumElements = MaxSize;
	srvDesc.Buffer.StructureByteStride = sizeof(float);
	descHeapManager->setSRV(baseIndex++, srvDesc, pDevice, *cbvResource);

	// Create resouce
	auto dim = inputTexture->getDimensions();
	if (scratchResource)
		scratchResource->resize(dim.x, dim.y);
	else
		scratchResource = &resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, dim.x, dim.y, scratchResFormat);

	// Describe resource and add to descriptor heap
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = scratchResFormat;
	descHeapManager->setUAV(baseIndex++, uavDesc, pDevice, *scratchResource);
}

void FilterComputeShader::updateData(DXCommandList currentCommandList)
{
	if (changed)
		cbvResource->write(currentCommandList, rendererResources->getTempResource(), linearFilterCoeff.data());
	changed = false;
}

void FilterComputeShader::dispatch(DXCommandList currentCommandList)
{
	auto dim = inputTexture->getDimensions();
	auto launchDimensions = getLaunchDimensions(dim);
	constexpr uint32_t arrSize = 2;
	uint32_t arr[arrSize] { 0u, static_cast<UINT>(linearFilterCoeff.size()) };

	outputTexture->uavBarrier(currentCommandList);
	scratchResource->uavBarrier(currentCommandList);

	currentCommandList->SetComputeRoot32BitConstants(1u, arrSize, &arr[0], 0u);
	currentCommandList->Dispatch(launchDimensions.x, launchDimensions.y, 1u);

	scratchResource->uavBarrier(currentCommandList);
	arr[0] = 1;

	currentCommandList->SetComputeRoot32BitConstants(1u, arrSize, &arr[0], 0u);
	currentCommandList->Dispatch(launchDimensions.x, launchDimensions.y, 1u);
}

void FilterComputeShader::initComponent(RendererResources* rendererResources, DXCommandList& pCurrentCommandList)
{
	cbvResource = &resourceManager->createResource(D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, static_cast<UINT>(MaxSize * sizeof(float)));
	cbvResource->setName("Gaussian Constant Data");
	cbvResource->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	changed = true;
}

void FilterComputeShader::setDxgiFormat(DXGI_FORMAT format)
{
	scratchResFormat = format;
}

void FilterComputeShader::setFiltersize(std::uint32_t filterSize)
{
	// Gaussian
	if (filterSize % 2 == 0)
		++filterSize;

	const float halfFilterSize = 0.5f * filterSize;
	const float stdDev = halfFilterSize / StdDeviations; // Working across 3 std deviations
	float f = -halfFilterSize;

	linearFilterCoeff.resize(filterSize);
	const float invTotalIntegral = 1.f / GaussIntegral(-halfFilterSize, halfFilterSize, stdDev);
	for (auto& lfc : linearFilterCoeff)
	{
		lfc = GaussIntegral(f, f + 1.f, stdDev) * invTotalIntegral;
		++f;
	}

	changed = true;
}

uint32_t FilterComputeShader::getFiltersize() const
{
	return static_cast<uint32_t>(linearFilterCoeff.size());
}

DistanceComputeShader::DistanceComputeShader()
	: SingleIOComputeShader("./Shaders/DistanceComputeShader.cso"), 
	  distConstBuffer(), faceIndexResource(), normalResource(), constBufferResource(), cdfMaskResource()
{
}

void DistanceComputeShader::addAdditionalResources(directx::RootSignatureManager* rsm, const std::string& rangeName)
{
	rsm->addDescriptorRange(rangeName, CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4u, 0u, 1u));
	rsm->addDescriptorRange(rangeName, CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1u, 0u, 1u));
}

void DistanceComputeShader::bindAdditionalResources(UINT baseIndex)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32G32_UINT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1u;
	faceIndexResource = resourceManager->getNamedResource("ltr_gMeshInfo");
	if (!constBufferResource)
		constBufferResource = &resourceManager->createResource(D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, sizeof(distConstBuffer));
	descHeapManager->setSRV(baseIndex++, srvDesc, pDevice, *faceIndexResource);
	
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	normalResource = resourceManager->getNamedResource("ltr_gNorm");
	descHeapManager->setSRV(baseIndex++, srvDesc, pDevice, *normalResource);

	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	cdfMaskResource = resourceManager->getNamedResource("ltr_cdf_mask");
	descHeapManager->setSRV(baseIndex++, srvDesc, pDevice, *cdfMaskResource);

	// Material
	srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.NumElements = static_cast<UINT>(rendererResources->scene->getMaterials().size());
	srvDesc.Buffer.StructureByteStride = sizeof(scene::Material);
	descHeapManager->setSRV(baseIndex++, srvDesc, pDevice, rendererResources->materialBuffer);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	const candela::directx::DXResource& res = *constBufferResource;
	cbvDesc.BufferLocation = res->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = sizeof(distConstBuffer);
	descHeapManager->setCBV(baseIndex++, cbvDesc, pDevice, res);

}

void DistanceComputeShader::updateData(DXCommandList currentCommandList)
{
	faceIndexResource->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	normalResource->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	cdfMaskResource->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	if (constBufferResource->getState() != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		constBufferResource->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	constBufferResource->write(currentCommandList, rendererResources->getTempResource(), &distConstBuffer);
}

void DistanceComputeShader::dispatch(DXCommandList currentCommandList)
{
	SingleIOComputeShader::dispatch(currentCommandList);
	faceIndexResource->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	normalResource->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cdfMaskResource->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void DistanceComputeShader::setMode(uint32_t mode)
{
	distConstBuffer.mode = mode;
}

uint32_t DistanceComputeShader::getMode() const
{
	return distConstBuffer.mode;
}
