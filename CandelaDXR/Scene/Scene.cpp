#include "Scene.h"

using std::vector;

using candela::scene::Texture;
using candela::scene::Material;
using candela::scene::Scene;

const vector<Texture>& Scene::getTextures() const
{
	return textures;
}

void Scene::addTexture(Texture texture)
{
	textures.push_back(std::move(texture));
}

const vector<Material>& Scene::getMaterials() const
{
	return materials;
}

void Scene::addMaterial(Material material)
{
	materials.push_back(std::move(material));

}
