#include "LTRasterGuidedShading.h"

using candela::renderer::LTRasterGuidedShading;

LTRasterGuidedShading::LTRasterGuidedShading()
	: rendererResources(), cdfSize(512, 512), rasterShader(true)
{
}

void LTRasterGuidedShading::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	rendererResources = rRes;
	rasterShader.setGlobaResourcePrefix("ltr_");
	rasterShader.resize(&cdfSize);
	rasterShader.init(rRes, pCurrentCommandList, resRegFn);
	cumulativeDistributionTexture = &rendererResources->resourceManager->createResource(
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		cdfSize.x, cdfSize.y,
		DXGI_FORMAT_R32_FLOAT, false, "cdf");

	distanceComputerShader.setAdditionalConstantBuffer(&rRes->camera->getPosition(), sizeof(float) * 3);
	distanceComputerShader.init(rRes, pCurrentCommandList);
	distanceComputerShader.setInputTexture("ltr_gPos");
	distanceComputerShader.setOutputTexture(cumulativeDistributionTexture);

	prefixSumComputeShader.init(rRes, pCurrentCommandList);
	prefixSumComputeShader.setInputTexture("ltr_gPos");
	prefixSumComputeShader.setOutputTexture(cumulativeDistributionTexture);

	normalisationComputeShader.init(rRes, pCurrentCommandList);
	normalisationComputeShader.setInputTexture("ltr_gPos");
	normalisationComputeShader.setOutputTexture(cumulativeDistributionTexture);
}

void LTRasterGuidedShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex)
{
	rasterShader.draw(pCurrentCommandList, currentBackBufferIndex);
	distanceComputerShader.compute(pCurrentCommandList);
	prefixSumComputeShader.compute(pCurrentCommandList);
	normalisationComputeShader.compute(pCurrentCommandList);
}

void LTRasterGuidedShading::accept(IVisitor* visitor)
{
	visitor->visit(this);
}

void LTRasterGuidedShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	rasterShader.onChange(pCurrentCommandList, currentBackBufferIndex, changeEvent);
}

void LTRasterGuidedShading::onResize()
{
	rasterShader.onResize();
}

bool LTRasterGuidedShading::isEnabled() const
{
	return false;
}

void LTRasterGuidedShading::setEnabled(bool p_enabled)
{
}

void LTRasterGuidedShading::appendToPipeline(directx::RootSignatureManager* rootSignatureManager)
{
}

void LTRasterGuidedShading::appendToShaderTable(directx::ShadingTable* shadingTable)
{
}

void LTRasterGuidedShading::appendToDescHeapManager(directx::DescriptorHeap* descriptorHeap)
{
}
