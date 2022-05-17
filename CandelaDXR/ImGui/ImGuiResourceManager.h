#pragma once

#include <vector>

#include "DirectX/ResourceManager.h"
#include "Renderer/RendererResources.h"

namespace candela::renderer::imgui
{
	class ImGuiResourceManager
	{
	public:
		ImGuiResourceManager(RendererResources* rRes, ID3D12DescriptorHeap* heap);

		void drawUi();
		void resize(UINT width, UINT height);

		// Constants
		static constexpr unsigned int MaxDisplayableResources = 4;
	private:
		directx::ResourceManager* resourceManager;
		ID3D12DescriptorHeap* heap;
		UINT descriptorSize;

		struct Item
		{
			std::string name;
			directx::Resource* resource;
			bool active;
		};

		std::vector<Item> resourceList;
	};
}