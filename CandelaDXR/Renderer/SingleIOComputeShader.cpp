#include <array>
#include <d3dcompiler.h>

#include "DirectX/RootSignatureManager.h"
#include "Exception/WindowException.h"
#include "Util/StringUtil.h"

#include "SingleIOComputeShader.h"

using std::string;
using std::uint32_t;

using candela::mathematics::UVector2;

using candela::directx::DescriptorHeap;
using candela::directx::RootSignatureManager;
using candela::renderer::SingleIOComputeShader;

SingleIOComputeShader::SingleIOComputeShader(const string& shaderPath, bool launchAsFlatArray)
	: shaderPath(shaderPath), launchAsFlatArray(launchAsFlatArray), resources(), numInputs(), numOutputs(), inputTextureIndex(), outputTextureIndex(), resourceManager(), pDevice(), cbData(), cbSize()
{
}

void SingleIOComputeShader::init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, std::vector<directx::Resource*>* res, uint32_t numOutputs, uint32_t numInputs)
{
	resourceManager = rendererResources->resourceManager.get();
	pDevice = rendererResources->pDevice.Get();
	resources = res;
	this->numInputs = numInputs;
	this->numOutputs = numOutputs;

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
	computeRootSignature = rsm->generateRootSignature("ComputeRootSignature", pDevice);

	// Create descriptor heap
	descHeapManager = std::make_unique<DescriptorHeap>(rsm, "IODescTable", "IO1", pDevice);

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
		uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
		for (uint32_t i = 0; i < numOutputs; ++i)
			descHeapManager->setUAV(entryNum++, uavDesc, pDevice, *(*resources)[i]);
	}

	// Load Shader
	HRESULT hr;
	wrl::ComPtr<ID3DBlob> pComputeBlob;
	GFXTHROWIFFAILED(D3DReadFileToBlob(util::StringToWString(shaderPath).c_str(), &pComputeBlob));

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
}

void SingleIOComputeShader::compute(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList)
{
	inputTexture->getAspectRatio();
	auto dim = inputTexture->getDimensions();

	ID3D12Resource* inputTextureResource = *inputTexture;
	ID3D12Resource* outputTextureResource = *outputTexture;
	if (outputTexture->getState() != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		ThrowException("Output Texture not in UAV state");
	std::array<UINT, 4> constants = { dim.x, dim.y, inputTextureIndex, outputTextureIndex };
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

void SingleIOComputeShader::dispatch(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList)
{
	auto dim = inputTexture->getDimensions();
	auto launchDimensions = getLaunchDimensions(dim);
	currentCommandList->Dispatch(launchDimensions.x, launchDimensions.y, 1u);
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
	scratchResource = &resourceManager->createResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, reqSize * sizeof(float));
	
	// Describe resource and add to descriptor heap
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.NumElements = reqSize;
	uavDesc.Buffer.StructureByteStride = sizeof(float);
	descHeapManager->setUAV(baseIndex, uavDesc, pDevice, *scratchResource);
}

void PrefixSumComputeShader::dispatch(wrl::ComPtr<ID3D12GraphicsCommandList> currentCommandList)
{
	auto dim = inputTexture->getDimensions();
	auto launchDimensions = getLaunchDimensions(dim);

	// First Pass - Perform Blelloch Scan on 2048 items per block (block size 1024)
	currentCommandList->SetComputeRoot32BitConstant(1u, 0u, 0u);
	currentCommandList->Dispatch(launchDimensions.x, launchDimensions.y, 1u);
	outputTexture->uavBarrier(currentCommandList);
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
