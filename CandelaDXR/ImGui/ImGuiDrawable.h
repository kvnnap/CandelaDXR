#pragma once

#include <cstddef>
#include <string>

#include <DirectXMath.h>

#include "Renderer/Drawable.h"
#include "ImGuiSceneNode.h"
#include "ImGuiMaterial.h"
#include "ImGuiShading.h"

namespace candela::renderer::imgui
{
	class ImGuiDrawable
		: public Drawable
	{
	public:
		ImGuiDrawable();
		~ImGuiDrawable();

		ChangeEvent_t processChangeEvent();

		// IDrawable interface
		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void accept(IVisitor* visitor) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
	private:
		// ImGui
		wrl::ComPtr<ID3D12DescriptorHeap> pImGuiDescriptorHeap;
		std::vector<imgui::ImGuiSceneNode> imguiSceneNodes;
		std::vector<imgui::ImGuiMaterial> imguiMaterials;
		std::vector<imgui::ImGuiShading> imguiShaders;

		RendererResources* rendererResources;
	};
}