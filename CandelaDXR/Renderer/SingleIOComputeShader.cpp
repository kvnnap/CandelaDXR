#include <array>
#include <d3dcompiler.h>

#include "DirectX/RootSignatureManager.h"
#include "Exception/WindowException.h"
#include "Util/StringUtil.h"

#include "SingleIOComputeShader.h"

using candela::mathematics::UVector2;

using candela::directx::DescriptorHeap;
using candela::directx::RootSignatureManager;
using candela::renderer::SingleIOComputeShader;

SingleIOComputeShader::SingleIOComputeShader(const std::string& shaderPath, bool launchAsFlatArray)
	: shaderPath(shaderPath), launchAsFlatArray(launchAsFlatArray), inputTexture(), outputTexture(), resourceManager(), pDevice(), cbData(), cbSize()
{
}

void SingleIOComputeShader::init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList)
{
	resourceManager = rendererResources->resourceManager.get();
	pDevice = rendererResources->pDevice.Get();

	// First need to generate Root Signature
	auto rsm = std::make_shared<RootSignatureManager>();
	CD3DX12_ROOT_PARAMETER1 param;
	rsm->addDescriptorRange("IORange", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1u, 0u));
	rsm->addDescriptorRange("IORange", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1u, 0u));
	rsm->setDescriptorTableParameter("IODescTable", "IORange");
	param.InitAsConstants(2u, 0u); rsm->setParameter("Constants", param);
	param.InitAsConstants(static_cast<UINT>(cbSize / 4), 1u); rsm->setParameter("Constants1", param);
	rsm->addParametersToRootSignature("ComputeRootSignature", { "Constants", "Constants1", "IODescTable" });
	computeRootSignature = rsm->generateRootSignature("ComputeRootSignature", pDevice);

	// Create descriptor heap
	descHeapManager = std::make_unique<DescriptorHeap>(rsm, "IODescTable", "IO1", pDevice);

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
	auto launchDimensions = getLaunchDimensions(dim);

	ID3D12Resource* inputTextureResource = *inputTexture;
	ID3D12Resource* outputTextureResource = *outputTexture;
	std::array<UINT, 2> constants = { dim.x, dim.y };
	auto prevState = inputTexture->getState();
	inputTexture->transistionBarrier(currentCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	currentCommandList->SetComputeRootSignature(computeRootSignature.Get());
	currentCommandList->SetPipelineState(computePipelineState.Get());
	currentCommandList->SetDescriptorHeaps(1u, descHeapManager->getDescriptorHeap().GetAddressOf());
	currentCommandList->SetComputeRoot32BitConstants(0u, static_cast<UINT>(constants.size()), constants.data(), 0u);
	if (cbData)
		currentCommandList->SetComputeRoot32BitConstants(1u, cbSize / 4u, cbData, 0u);
	currentCommandList->SetComputeRootDescriptorTable(2u, descHeapManager->getDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	currentCommandList->Dispatch(launchDimensions.x, launchDimensions.y, 1u);
	inputTexture->transistionBarrier(currentCommandList, prevState);
	outputTexture->uavBarrier(currentCommandList);
}

void SingleIOComputeShader::setInputTexture(directx::Resource* p_inputTexture)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	//srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1u;

	inputTexture = p_inputTexture;
	descHeapManager->setSRV(0u, srvDesc, pDevice, *inputTexture);
}

void SingleIOComputeShader::setInputTexture(const std::string& inputTextureName)
{
	setInputTexture(resourceManager ? resourceManager->getNamedResource(inputTextureName) : nullptr);
}

void SingleIOComputeShader::setOutputTexture(directx::Resource* p_outputTexture)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
	outputTexture = p_outputTexture;
	descHeapManager->setUAV(1u, uavDesc, pDevice, *outputTexture);
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
