#pragma once

#include <cstddef>
#include <string>
#include <memory>

#include <DirectXMath.h>

#include "Renderer/Drawable.h"
#include "ImGuiSceneNode.h"
#include "ImGuiMaterial.h"
#include "ImGuiShading.h"
#include "ImGuiRenderer.h"
#include "ImGuiResourceManager.h"

namespace candela::renderer::imgui
{
	class ImGuiManager
		: public Drawable
	{
	public:
		ImGuiManager();
		~ImGuiManager();

		ChangeEvent_t processChangeEvent();
		directx::Resource* getResourceToSave() const;

		// IDrawable interface
		void init(RendererResources* rendererResources, directx::DXCommandList& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(directx::DXCommandList pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void accept(IVisitor* visitor) override;
		void onChange(directx::DXCommandList pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
	private:
		// ImGui
		wrl::ComPtr<ID3D12DescriptorHeap> pImGuiDescriptorHeap;
		std::unique_ptr<ImGuiSceneNode> imguiRootSceneNode;
		std::vector<ImGuiMaterial> imguiMaterials;
		std::vector<ImGuiShading> imguiShaders;
		std::unique_ptr<ImGuiResourceManager> imguiResourceManager;
		std::unique_ptr<ImGuiRenderer> imguiRenderer;

		RendererResources* rendererResources;
	};
}