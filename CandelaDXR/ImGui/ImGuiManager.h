#pragma once

#include <cstddef>
#include <string>
#include <memory>

#include <DirectXMath.h>

#include "Renderer/Drawable.h"
#include "ImGuiSceneNode.h"
#include "ImGuiMaterial.h"
#include "ImGuiShading.h"
#include "ImGuiResourceManager.h"

namespace candela::renderer::imgui
{
	class ImGuiManager
		: public Drawable
	{
	public:
		~ImGuiManager();

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
		std::vector<ImGuiSceneNode> imguiSceneNodes;
		std::vector<ImGuiMaterial> imguiMaterials;
		std::vector<ImGuiShading> imguiShaders;
		std::unique_ptr<ImGuiResourceManager> imguiResourceManager;

		RendererResources* rendererResources;
	};
}