#include "DirectX/DxUtil.h"

#include "LTOptimisedComponent.h"

using candela::renderer::LTOptimisedComponent;

using candela::directx::DXUtil;

LTOptimisedComponent::LTOptimisedComponent()
	: rendererResources(), constBuffer()
{
}

void LTOptimisedComponent::setCausticsRatio(float p_causticsRatio)
{
	constBuffer.causticsRatio = p_causticsRatio;
}

float LTOptimisedComponent::getCausticsRatio() const
{
	return constBuffer.causticsRatio;
}

void LTOptimisedComponent::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	rendererResources = rRes;
	constBuffer.numSpeculars = static_cast<uint32_t>(rRes->scene->getSpeculars().size());

	// Constant buffer
	constantBuffer = DXUtil::uploadDataToDefaultHeap(rendererResources->pDevice, pCurrentCommandList, rendererResources->getTempResource(), &constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	constantBuffer->SetName(L"LT Optimised Constant Buffer");
}

void LTOptimisedComponent::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex)
{
	DXUtil::updateDataInDefaultHeap(rendererResources->pDevice, pCurrentCommandList, constantBuffer, rendererResources->getTempResource(),
		&constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void LTOptimisedComponent::accept(IVisitor* visitor)
{
	visitor->visit(this);
}

void LTOptimisedComponent::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::SceneChange))
		constBuffer.numSpeculars = static_cast<uint32_t>(rendererResources->scene->getSpeculars().size());
}

void LTOptimisedComponent::onResize()
{
}

bool LTOptimisedComponent::isEnabled() const
{
	return false;
}

void LTOptimisedComponent::setEnabled(bool p_enabled)
{
}

bool LTOptimisedComponent::shouldClearAccumulation() const
{
	return false;
}

std::uint32_t candela::renderer::LTOptimisedComponent::getBufferUsage() const
{
	return BufferUsage::Diffuse;
}

void LTOptimisedComponent::appendToPipeline(directx::RootSignatureManager* rootSignatureManager)
{
	CD3DX12_ROOT_PARAMETER1 param;
	param.InitAsConstantBufferView(1); rootSignatureManager->setParameter("OptimisedConstBuff", param);
	rootSignatureManager->addParameterToRootSignature("RayGenRootSignature", "OptimisedConstBuff");
}

void LTOptimisedComponent::appendToShaderTable(directx::ShadingTable* shadingTable)
{
	shadingTable->setInputForViewParameter(L"rayGen", "OptimisedConstBuff", constantBuffer);
}

void LTOptimisedComponent::appendToDescHeapManager(directx::DescriptorHeap* descriptorHeap)
{
}

const char* LTOptimisedComponent::getName() const
{
	return "Unnamed";
}
