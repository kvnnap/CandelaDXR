#include "LTRasterGuidedShading.h"

using candela::renderer::LTRasterGuidedShading;

LTRasterGuidedShading::LTRasterGuidedShading()
	: rendererResources(), rasterShader(true)
{
}

void LTRasterGuidedShading::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	rendererResources = rRes;
	rasterShader.setGlobaResourcePrefix("ltr_");
	rasterShader.init(rRes, pCurrentCommandList, resRegFn);
}

void LTRasterGuidedShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex)
{
	rasterShader.draw(pCurrentCommandList, currentBackBufferIndex);
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
