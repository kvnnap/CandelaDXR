#include "imgui/imgui.h"

#include "ImGuiResourceManager.h"

using candela::directx::ResourceManager;
using candela::directx::Resource;
using candela::renderer::imgui::ImGuiResourceManager;

ImGuiResourceManager::ImGuiResourceManager(RendererResources* rRes, ID3D12DescriptorHeap* heap)
	: resourceManager(rRes->resourceManager.get()), heap(heap), resourceToSave()
{
	descriptorSize = rRes->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (const auto& res : resourceManager->getNamedResources())
		resourceList.push_back(Item{ res.first, res.second, false });
}

void ImGuiResourceManager::drawUi()
{
	auto cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
	auto gpuHandle = heap->GetGPUDescriptorHandleForHeapStart();

	ImGui::PushID(this);
	UINT activeCount{};
	UINT displayCount{};
	bool changed{};
	resourceToSave = nullptr;

	// First pass
	for (auto& res : resourceList)
	{
		if (ImGui::Checkbox(res.name.c_str(), &res.active))
			changed = true;
		ImGui::SameLine();

		ImGui::PushID(res.resource);
		if (ImGui::Button("Save"))
			resourceToSave = res.resource;
		ImGui::PopID();

		if (res.active)
			++activeCount;
		if (activeCount > ImGuiResourceManager::MaxDisplayableResources)
			res.active = false;
		displayCount++;
		if (displayCount % 2 == 1 && displayCount < resourceList.size())
			ImGui::SameLine();
	}
	
	// Second pass
	displayCount = 0;
	for (auto& res : resourceList)
	{
		if (!res.active)
			continue;

		cpuHandle.ptr += descriptorSize;
		gpuHandle.ptr += descriptorSize;
		// Update heap only if it changed
		if (changed)
			res.resource->createShaderResourceView(cpuHandle);

		float aspectRatio = res.resource->getAspectRatio();
		auto itemWidth = ImGui::GetWindowSize().x * (activeCount > 1 ? .5f : 1.f) - 20;
		ImGui::Image((ImTextureID)gpuHandle.ptr, { itemWidth, itemWidth / aspectRatio }, { 0, 0 }, { 1,1 }, { 1,1,1,1 }, { 0,0,1,1 });
		if (++displayCount % 2 == 1)
			ImGui::SameLine();
	}
	

	ImGui::PopID();
}

void ImGuiResourceManager::resize(UINT width, UINT height)
{
	auto cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
	auto gpuHandle = heap->GetGPUDescriptorHandleForHeapStart();

	for (auto& res : resourceList)
	{
		if (!res.active)
			continue;

		cpuHandle.ptr += descriptorSize;
		gpuHandle.ptr += descriptorSize;
		// Update heap only if it changed
		res.resource->createShaderResourceView(cpuHandle);
	}
}

Resource* ImGuiResourceManager::getResourceToSave() const
{
	return resourceToSave;
}
