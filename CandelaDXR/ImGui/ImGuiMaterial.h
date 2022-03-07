#pragma once

#include <DirectXMath.h>

#include "Scene/Scene.h"

namespace candela::renderer::imgui
{
	class ImGuiMaterial
	{
	public:
		ImGuiMaterial(scene::Material &material, std::size_t materialId);

		void drawUi();

		bool hasChanged() const;
	private:
		scene::Material& material;
		std::size_t materialId;

		bool changed;
	};
}