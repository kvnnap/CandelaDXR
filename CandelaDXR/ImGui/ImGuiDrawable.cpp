#include "imgui/imgui.h"
#include "ImGui/Backend/imgui_impl_win32.h"
#include "ImGui/Backend/imgui_impl_dx12.h"

#include "DirectX/d3dx12.h"
#include "DirectX/DxUtil.h"

#include "ImGuiDrawable.h"

using candela::directx::DXUtil;
using candela::renderer::imgui::ImGuiDrawable;
using candela::renderer::ChangeEvent_t;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiDrawable::ImGuiDrawable()
{
}

ImGuiDrawable::~ImGuiDrawable()
{
	// Cleanup ImGui
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
}

ChangeEvent_t ImGuiDrawable::processChangeEvent()
{
	ChangeEvent_t changeEvent{};
	if (!isEnabled())
		return changeEvent;

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Transforms");
	for (auto& imguiSceneNode : imguiSceneNodes)
	{
		imguiSceneNode.drawUi();
		if (imguiSceneNode.hasChanged())
			changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::Transformation);
	}
	ImGui::End();

	ImGui::Begin("Materials");
	for (auto& imguiMaterial : imguiMaterials)
	{
		imguiMaterial.drawUi();
		if (imguiMaterial.hasChanged())
			changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::SceneUpdate);
		if (imguiMaterial.hasMajorChange())
			changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::SceneChange);
	}
	ImGui::End();

	ImGui::Begin("Shaders");
	for (auto& imguiShader : imguiShaders)
	{
		imguiShader.drawUi();
		if (imguiShader.hasChanged())
			changeEvent |= static_cast<ChangeEvent_t>(ChangeEvent::Statistics);
	}
	ImGui::End();
	ImGui::Render();

	return changeEvent;
}

void ImGuiDrawable::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	rendererResources = rRes;

	// ImGui
	pImGuiDescriptorHeap = DXUtil::createDescriptorHeap(rRes->pDevice, 1u, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	pImGuiDescriptorHeap->SetName(L"ImGui Descriptor Heap");
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(rRes->window->getHandle());
	ImGui_ImplDX12_Init(rRes->pDevice.Get(), rRes->numBackBuffers, DXGI_FORMAT_R8G8B8A8_UNORM, pImGuiDescriptorHeap.Get(),
		pImGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		pImGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	ImGui::StyleColorsDark();
	rRes->window->addWndProcCallback(ImGui_ImplWin32_WndProcHandler, TRUE);
	auto scene = rRes->scene;
	for (auto& child : scene->getSceneGraph().Children)
		imguiSceneNodes.emplace_back(child, *scene);
	for (size_t i = 0; i < scene->getMaterials().size(); ++i)
	{
		auto& mat = scene->getMaterials()[i];
		imguiMaterials.emplace_back(mat, i, scene->getMaterialName(i));
	}
	for (auto drawable : *rRes->drawables)
		imguiShaders.emplace_back(drawable);
}

void ImGuiDrawable::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex)
{
	if (!isEnabled())
		return;

	const auto incrementSize = rendererResources->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto rtvDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(rendererResources->pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), currentBackBufferIndex, incrementSize);
	pCurrentCommandList->OMSetRenderTargets(1u, &rtvDescriptorHandle, FALSE, nullptr);
	pCurrentCommandList->SetDescriptorHeaps(1u, pImGuiDescriptorHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCurrentCommandList.Get());
}

void ImGuiDrawable::accept(IVisitor* visitor)
{
}

void ImGuiDrawable::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
}

void ImGuiDrawable::onResize()
{
}
