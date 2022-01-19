#pragma once

#include <cstdint>
#include <vector>

#include "Texture.h"
#include "Material.h"

namespace candela::scene
{
	class Scene
	{
	public:
		//const Texture& getTexture(std::size_t id) const;
		//std::size_t getNumberOfTextures() const;

		const std::vector<Texture>& getTextures() const;
		void addTexture(Texture texture);

		const std::vector<Material>& getMaterials() const;
		void addMaterial(Material texture);

	private:
		std::vector<Texture> textures;
		std::vector<Material> materials;
	};
}
