#pragma once

#include <cstddef>
#include <string>

#include <DirectXMath.h>

#include "Scene/Scene.h"

namespace candela::renderer::imgui
{
	class ImGuiMaterial
	{
	public:
		ImGuiMaterial(scene::Material &material, std::size_t materialId, const std::string &materialName);

		void drawUi();

		bool hasChanged() const;

		// A major change occurs if material becomes or stops become a light or specular
		bool hasMajorChange() const;
	private:
		scene::Material& material;
		std::size_t materialId;
		std::string materialName;

		bool changed;
		bool majorChange;
		bool directionalEmissive;
	};
}