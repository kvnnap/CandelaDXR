#include <stdexcept>

#include "Scene.h"

using std::array;
using std::vector;
using std::string;
using std::size_t;
using std::uint32_t;
using std::runtime_error;

using candela::mathematics::Vector2;
using candela::mathematics::Vector3;

using candela::scene::Texture;
using candela::scene::Material;
using candela::scene::Scene;
using candela::scene::SceneNode;
using candela::scene::AreaLight;
using candela::scene::FaceAttributes;
using candela::scene::IndexedSpan;

Scene::Scene()
{
	sceneGraph.NodeName = "_root_";
	sceneGraph.Transform = DirectX::XMMatrixIdentity();
}

size_t Scene::addTexture(Texture texture)
{
	textures.push_back(std::move(texture));
	return textures.size() - 1;
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
	currentGroupName = name;
	spanDataMap.insert({ name, { name, indexData.size(), 0 } });
}

void Scene::endGroup()
{
	if (currentGroupName.empty())
		return;

	// Allocate enough space in vertex, tex and normal buffers
	std::size_t newSize = vertices.size() + collisionMap.size();
	vertices.resize(newSize);
	textureCoords.resize(newSize);
	normals.resize(newSize);

	// Transfer vertices from collision map to buffers
	auto it = collisionMap.begin();
	while (it != collisionMap.end())
	{
		auto& key = it->first;
		auto index = it->second;
		vertices[index] = Vector3(key[0], key[1], key[2]);
		textureCoords[index] = Vector2(key[3], key[4]);
		normals[index] = Vector3(key[5], key[6], key[7]);
		collisionMap.erase(it++);
	}

	auto& index = spanDataMap[currentGroupName];
	index.Size = indexData.size() - index.Start;
	currentGroupName.clear();
}

void candela::scene::Scene::addFace(
	const array<Vector3, 3>& pos, 
	const array<Vector2, 3>& tex,
	const array<Vector3, 3>& norm,
	uint32_t materialId)
{
	// Add mesh data and vector attributes
	for (auto i = 0; i < 3; ++i)
	{
		array<float, 8> arr = { pos[i].x, pos[i].y, pos[i].z, tex[i].x, tex[i].y, norm[i].x, norm[i].y, norm[i].z };
		auto it = collisionMap.find(arr);
		if (it == collisionMap.end())
		{
			auto index = static_cast<int>(vertices.size() + collisionMap.size());
			collisionMap.insert({ arr, index });
			indexData.push_back(index);
		}
		else
		{
			indexData.push_back(it->second);
		}
	}

	// Is light?
	if (materials[materialId].isEmissive())
	{
		lights.emplace_back(AreaLight{
			.Intensity = DirectX::XMVectorSet(1.f, 1.f, 1.f, 1.f),
			.InstanceIndex = static_cast<uint32_t>(spanDataMap.size() - 1),
			.PrimitiveId = static_cast<uint32_t>(indexData.size() / 3 - 1),
			.MaterialId = materialId
		});
	}

	// Add face attributes
	faceAttributes.emplace_back(FaceAttributes{
		.MaterialId = materialId,
		.AreaLightId = materials[materialId].isEmissive() ? static_cast<uint32_t>(lights.size() - 1) : 0,
		.InstanceIndex = static_cast<uint32_t>(spanDataMap.size() - 1)
	});
}

void Scene::recalculateLightsAndFaceAttributes()
{
	lights.clear();
	for (uint32_t i = 0; i < faceAttributes.size(); ++i)
	{
		auto& fAttr = faceAttributes[i];
		const auto& mat = materials[fAttr.MaterialId];
		if (mat.isEmissive())
		{
			fAttr.AreaLightId = lights.size();
			lights.emplace_back(AreaLight{
				.Intensity = DirectX::XMVectorSet(1.f, 1.f, 1.f, 1.f),
				.InstanceIndex = fAttr.InstanceIndex,
				.PrimitiveId = i,
				.MaterialId = fAttr.MaterialId
			});
		}
	}
}

void Scene::addSceneNodeToGroupMapping(const string& sceneNodeName, const string& groupName)
{
	if (spanDataMap.find(groupName) == spanDataMap.end())
		return;
	sceneGraph.addChild(sceneNodeName, groupName);
}

bool candela::scene::Material::isEmissive() const
{
	return Emissive.x != 0.f || Emissive.y != 0.f || Emissive.z != 0.f;
}

void candela::scene::SceneNode::addChild(const string& nodeName, const string& groupName)
{
	// Or throw
	for (auto& child : Children)
		if (child.NodeName == nodeName)
			return;

	// Add the mapping
	Children.emplace_back(SceneNode{
		.Parent = this,
		.Transform = DirectX::XMMatrixIdentity(),
		.NodeName = nodeName,
		.GroupName = groupName
	});
}

// Getters
const vector<Vector3>& Scene::getVertices() const { return vertices; }
const vector<Vector2>& Scene::getTextureCoords() const { return textureCoords; }
const vector<Vector3>& Scene::getNormals() const { return normals; }
const vector<int>& Scene::getIndices() const { return indexData; }

const vector<Texture>& Scene::getTextures() const { return textures; }
const vector<Material>& Scene::getMaterials() const { return materials; }
vector<Material>& Scene::getMaterials() { return materials; }
const vector<AreaLight>& Scene::getLights() const { return lights; }
const vector<FaceAttributes>& Scene::getFaceAttributes() const { return faceAttributes; }

//vector<const IndexedSpan*> Scene::getMeshIndexedSpans() const
//{
//	vector<const IndexedSpan*> list;
//	for (auto& item : spanDataMap)
//		list.push_back(&item.second);
//	return list;
//}

const IndexedSpan& Scene::getMeshIndexedSpan(const string& groupName) const
{
	return spanDataMap.at(groupName);
}

const std::unordered_map<std::string, IndexedSpan>& Scene::getMeshIndexedSpanDataMap() const
{
	return spanDataMap;
}

const SceneNode& Scene::getSceneGraph() const { return sceneGraph; }
SceneNode& Scene::getSceneGraph() { return sceneGraph; }

const size_t Scene::getVerticesOffset() const
{
	return 0;
}

const size_t Scene::getVerticesSizeBytes() const
{
	return vertices.size() * sizeof(Vector3);
}

const size_t Scene::getTextureCoordsOffset() const
{
	return getVerticesOffset() + getVerticesSizeBytes();
}

const size_t Scene::getTextureCoordsSizeBytes() const
{
	return textureCoords.size() * sizeof(Vector2);
}

const size_t Scene::getNormalsOffset() const
{
	return getTextureCoordsOffset() + getTextureCoordsSizeBytes();
}

const size_t Scene::getNormalsSizeBytes() const
{
	return normals.size() * sizeof(Vector3);
}

const size_t Scene::getIndicesOffset() const
{
	return getNormalsOffset() + getNormalsSizeBytes();
}

const size_t Scene::getIndicesSizeBytes() const
{
	return indexData.size() * sizeof(int);
}