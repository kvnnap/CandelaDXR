#include "Scene.h"

#include <stdexcept>

using std::vector;
using std::string;
using std::size_t;
using std::runtime_error;

using candela::scene::Texture;
using candela::scene::Material;
using candela::scene::Scene;

Scene::Scene()
{
	sceneGraph.NodeName = "_root_";
	sceneGraph.Transform = DirectX::XMMatrixIdentity();
}

const vector<Texture>& Scene::getTextures() const
{
	return textures;
}

size_t Scene::addTexture(Texture texture)
{
	textures.push_back(std::move(texture));
	return textures.size() - 1;
}

const vector<Material>& Scene::getMaterials() const
{
	return materials;
}

void Scene::addMaterial(Material material)
{
	materials.push_back(std::move(material));

}

void Scene::startGroup(const string& name)
{
	if (!currentGroupName.empty())
		endGroup();
	if (spanDataMap.find(name) != spanDataMap.end())
		throw runtime_error("Scene group " + string(name) + " already exists");
	spanDataMap[name] = { name, indexData.size(), 0 };
}

void Scene::endGroup()
{
	if (currentGroupName.empty())
		return;
	// Transfer vertices from collission map to buffers
	// TODO
	auto& index = spanDataMap[currentGroupName];
	index.Size = indexData.size() - index.Start;
	currentGroupName.clear();
}

void candela::scene::Scene::addFace(
	const std::array<mathematics::Vector3, 3>& pos, 
	const std::array<mathematics::Vector2, 3>& tex,
	const std::array<mathematics::Vector3, 3>& norm,
	std::uint32_t materialId)
{

}
