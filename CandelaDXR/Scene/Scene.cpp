#include <stdexcept>

#include "Scene.h"

#include "Exception/Exception.h"

using std::array;
using std::vector;
using std::string;
using std::size_t;
using std::uint32_t;
using std::runtime_error;

using candela::mathematics::Vector2;
using candela::mathematics::Vector3;
using candela::mathematics::Matrix;

using candela::scene::Texture;
using candela::scene::Material;
using candela::scene::Scene;
using candela::scene::SceneNode;
using candela::scene::AreaLight;
using candela::scene::SpecularPrimitive;
using candela::scene::FaceAttributes;
using candela::scene::IndexedSpan;
using candela::renderer::Camera;

Scene::Scene()
{
	sceneGraph.NodeName = "_root_";
	sceneGraph.Transform = DirectX::XMMatrixIdentity();
}

size_t Scene::addTexture(std::unique_ptr<Texture> texture)
{
	textures.push_back(std::move(texture));
	return textures.size() - 1;
}

void Scene::addMaterial(Material material, const string& name)
{
	materials.push_back(std::move(material));
	materialNames.push_back(name);
}

void Scene::startGroup(const string& name)
{
	if (!currentGroupName.empty())
		endGroup();
	if (spanDataMap.find(name) != spanDataMap.end())
		ThrowException("Scene group " + string(name) + " already exists");
	currentGroupName = name;
	posAccumulator = {};
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
	const float invSize = 1.f / index.Size;
	index.CentrePosition = DirectX::XMVectorMultiply(posAccumulator, DirectX::XMVectorSet(invSize, invSize, invSize, invSize));
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
		posAccumulator = DirectX::XMVectorAdd(posAccumulator, DirectX::XMLoadFloat3(&pos[i]));
		
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
			.InstanceIndex = static_cast<uint32_t>(spanDataMap.size() - 1),
			.PrimitiveId = static_cast<uint32_t>(indexData.size() / 3 - 1),
			.MaterialId = materialId
		});
	}

	// If specular?
	if (materials[materialId].isSpecular())
	{
		speculars.emplace_back(SpecularPrimitive{
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

// This function should be called when changes in lights and speculars are known to occur
void Scene::recalculateLightsAndFaceAttributes()
{
	lights.clear();
	speculars.clear();
	for (uint32_t i = 0; i < faceAttributes.size(); ++i)
	{
		auto& fAttr = faceAttributes[i];
		const auto& mat = materials[fAttr.MaterialId];
		if (mat.isEmissive())
		{
			fAttr.AreaLightId = static_cast<uint32_t>(lights.size());
			lights.emplace_back(AreaLight{
				.InstanceIndex = fAttr.InstanceIndex,
				.PrimitiveId = i,
				.MaterialId = fAttr.MaterialId
			});
		}
		if (mat.isSpecular())
		{
			speculars.emplace_back(SpecularPrimitive{
				.InstanceIndex = fAttr.InstanceIndex,
				.PrimitiveId = i,
				.MaterialId = fAttr.MaterialId
			});
		}
	}
}

SceneNode& Scene::addSceneNodeToGroupMapping(SceneNode& sceneNode, const string& sceneNodeName, const string& groupName)
{
	DirectX::XMVECTOR centrePosition{};
	auto item = spanDataMap.find(groupName);
	if (item != spanDataMap.end())
		centrePosition = item->second.CentrePosition;
	return sceneNode.addChild(sceneNodeName, groupName, centrePosition);
}

bool Material::isEmissive() const
{
	return Emissive.x != 0.f || Emissive.y != 0.f || Emissive.z != 0.f;
}

bool Material::isSpecular() const
{
	return Dissolve < 1.f;
}

bool SceneNode::isLeaf() const
{
	return Children.empty();
}

SceneNode& SceneNode::addChild(const string& nodeName, const string& groupName, const DirectX::XMVECTOR& centrePos)
{
	// Or throw
	for (auto& child : Children)
		if (child->NodeName == nodeName)
			throw std::runtime_error("child nodeName already exists");

	// Add the mapping
	auto &ref = Children.emplace_back(std::make_unique<SceneNode>());
	ref->Parent = this;
	ref->Transform = DirectX::XMMatrixIdentity();
	ref->CentrePosition = centrePos;
	ref->NodeName = nodeName;
	ref->GroupName = groupName;
	return *ref;
}

Matrix SceneNode::getTransform() const
{
	Matrix mat = Transform;
	const SceneNode* sceneNode = Parent;
	while (sceneNode)
	{
		mat *= sceneNode->Transform;
		sceneNode = sceneNode->Parent;
	}
	return mat;
}

void SceneNode::processCentrePositionsForDirectChildren()
{
	if (isLeaf())
		return;
	DirectX::XMVECTOR accum{};
	for (auto& snChild : Children)
		accum = DirectX::XMVectorAdd(accum, snChild->CentrePosition);
	const float invSize = 1.f / Children.size();
	CentrePosition = DirectX::XMVectorMultiply(accum, DirectX::XMVectorSet(invSize, invSize, invSize, invSize));
}

void SceneNode::getLeafNodes(vector<SceneNode*>& leafs)
{
	if (isLeaf() && !GroupName.empty())
	{
		leafs.push_back(this);
		return;
	}

	for (auto& child : Children)
		child->getLeafNodes(leafs);
}

vector<SceneNode*> SceneNode::getLeafNodes()
{
	vector<SceneNode*> leafs;
	getLeafNodes(leafs);
	return leafs;
}

void SceneNode::getAllNodes(vector<SceneNode*>& nodes)
{
	nodes.push_back(this);
	for (auto& child : Children)
		child->getAllNodes(nodes);
}

vector<SceneNode*> SceneNode::getAllNodes()
{
	vector<SceneNode*> nodes;
	getAllNodes(nodes);
	return nodes;
}

void SceneNode::transform(const Matrix& trans)
{
	Transform = trans;
}

const DirectX::XMVECTOR& SceneNode::getCentrePosition() const
{
	return CentrePosition;
}

// Getters
const vector<Vector3>& Scene::getVertices() const { return vertices; }
const vector<Vector2>& Scene::getTextureCoords() const { return textureCoords; }
const vector<Vector3>& Scene::getNormals() const { return normals; }
const vector<int>& Scene::getIndices() const { return indexData; }

const vector<std::unique_ptr<Texture>>& Scene::getTextures() const { return textures; }
const vector<Material>& Scene::getMaterials() const { return materials; }
string Scene::getMaterialName(size_t matId) const { return materialNames.at(matId); }
vector<Material>& Scene::getMaterials() { return materials; }
const vector<AreaLight>& Scene::getLights() const { return lights; }
const vector<SpecularPrimitive>& Scene::getSpeculars() const { return speculars; }
const vector<FaceAttributes>& Scene::getFaceAttributes() const { return faceAttributes; }

const IndexedSpan& Scene::getMeshIndexedSpan(const string& groupName) const
{
	return spanDataMap.at(groupName);
}

const std::unordered_map<string, IndexedSpan>& Scene::getMeshIndexedSpanDataMap() const
{
	return spanDataMap;
}

const SceneNode& Scene::getSceneGraph() const { return sceneGraph; }
SceneNode& Scene::getSceneGraph() { return sceneGraph; }

void Scene::addCamera(Camera camera)
{
	cameras.push_back(camera);
}

const std::vector<Camera>& Scene::getCameras() const
{
	return cameras;
}

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