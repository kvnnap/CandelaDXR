#include <stdexcept>
#include <ranges>

#include "Scene.h"

#include "Exception/Exception.h"

using std::array;
using std::vector;
using std::string;
using std::size_t;
using std::uint32_t;
using std::runtime_error;

using candela::mathematics::Vector;
using candela::mathematics::Vector2;
using candela::mathematics::Vector3;
using candela::mathematics::Matrix;
using candela::mathematics::AABB;

using candela::scene::Texture;
using candela::scene::Material;
using candela::scene::Scene;
using candela::scene::SceneNode;
using candela::scene::SingleMeshSceneNode;
using candela::scene::AreaLight;
using candela::scene::Light;
using candela::scene::SpecularPrimitive;
using candela::scene::FaceAttributes;
using candela::scene::IndexedSpan;
using candela::scene::AnimationRecord;
using candela::renderer::Camera;

Scene::Scene()
	: sceneGraph(*this)
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

void Scene::startMesh(const string& meshName)
{
	posAccumulator = {};
	aabbAccum = mathematics::AABB();
	meshes.emplace_back(IndexedSpan{ meshName, indexData.size(), 0 });
}

size_t Scene::endMesh()
{
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

	auto& index = meshes.back();
	index.Size = indexData.size() - index.Start;
	const float invSize = 1.f / index.Size;
	index.CentrePosition = DirectX::XMVectorMultiply(posAccumulator, DirectX::XMVectorSet(invSize, invSize, invSize, invSize));
	index.AxisAlignedBB = aabbAccum;

	return meshes.size() - 1;
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
		auto vPos = DirectX::XMLoadFloat3(&pos[i]);
		posAccumulator = DirectX::XMVectorAdd(posAccumulator, vPos);
		vPos.m128_f32[3] = 1.f;
		aabbAccum.contain(vPos);

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

	// Add face attributes
	faceAttributes.emplace_back(FaceAttributes{
		.MaterialId = materialId,
		.MeshIndex = static_cast<uint32_t>(meshes.size() - 1)
	});
}

// This function should be called when changes in lights and speculars are known to occur
void Scene::recalculateLightsAndFaceAttributes()
{
	lights.clear();
	speculars.clear();
	
	vector<vector<AreaLight>> meshLights; // Maps the real mesh with Emissive Area Lights - prepass
	vector<vector<SpecularPrimitive>> meshSpecs; // Maps the real mesh with Speculars - prepass
	meshLights.resize(meshes.size());
	meshSpecs.resize(meshes.size());

	// Pre-pass
	for (uint32_t i = 0; i < faceAttributes.size(); ++i)
	{
		auto& fAttr = faceAttributes[i];
		const auto& mat = materials[fAttr.MaterialId];
		if (mat.isEmissive())
		{
			meshLights[fAttr.MeshIndex].emplace_back(AreaLight{
				.PrimitiveId = i,
				.MaterialId = fAttr.MaterialId
			});
		}
		if (mat.isSpecular())
		{
			meshSpecs[fAttr.MeshIndex].emplace_back(SpecularPrimitive{
				.PrimitiveId = i,
				.MaterialId = fAttr.MaterialId
			});
		}
	}

	// Process
	std::uint32_t i = 0;
	for (const auto& meshInstance : sceneGraph.getFlattenedMeshNodes())
	{
		for (auto& meshLight : meshLights[meshInstance.MeshId])
		{
			lights.emplace_back(AreaLight{
				.InstanceIndex = i,
				.PrimitiveId = meshLight.PrimitiveId,
				.MaterialId = meshLight.MaterialId
			});
		}
		for (auto& meshSpec : meshSpecs[meshInstance.MeshId])
		{
			speculars.emplace_back(SpecularPrimitive{
				.InstanceIndex = i,
				.PrimitiveId = meshSpec.PrimitiveId,
				.MaterialId = meshSpec.MaterialId
			});
		}
		++i;
	}
}

void Scene::addExternalLight(const LightNode& light)
{
	const auto lightType = light.Light.Type;
	if (lightType == LT_UNDEFINED || lightType == LT_AMBIENT || lightType == LT_AREA || lightType > LT_AREA)
		ThrowException("Adding unsupported Light Type: " + std::to_string(lightType));
	externalLights.push_back(light);
}

void Scene::addAnimation(std::unique_ptr<animation::IAnimation> animation)
{
	animations.push_back(std::move(animation));
}

AnimationRecord& Scene::addAnimationRecord()
{
	return animationRecords.emplace_back();
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

SceneNode& SceneNode::getRootNode()
{
	auto node = this;
	while (node->Parent)
		node = node->Parent;
	return *node;
}

SceneNode* SceneNode::getNode(const std::string& nodeName)
{
	if (nodeName == NodeName)
		return this;

	for (auto& child : Children)
	{
		if (auto ret = child->getNode(nodeName))
			return ret;
	}

	return nullptr;
}

SceneNode::SceneNode(scene::Scene& scene, SceneNode* parent)
	: Scene(scene), Parent(parent)
{
	NodeId = assignNewNodeId();
}

SceneNode& SceneNode::addChild(const string& nodeName)
{
	// Add the mapping
	auto& ref = Children.emplace_back(std::make_unique<SceneNode>(Scene, this));
	ref->Transform = DirectX::XMMatrixIdentity();
	ref->NodeName = nodeName;
	return *ref;
}

size_t SceneNode::assignNewNodeId()
{
	return ++getRootNode().NextNodeId;
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

void SceneNode::getMeshNodes(vector<SceneNode*>& meshNodes)
{
	if (!Meshes.empty())
		meshNodes.push_back(this);

	for (auto& child : Children)
		child->getMeshNodes(meshNodes);
}

vector<SceneNode*> SceneNode::getMeshNodes()
{
	vector<SceneNode*> meshNodes;
	meshNodes.reserve(getRootNode().NextNodeId);
	getMeshNodes(meshNodes);
	return meshNodes;
}

vector<SingleMeshSceneNode> SceneNode::getFlattenedMeshNodes()
{
	vector<SingleMeshSceneNode> nodes;
	nodes.reserve(getRootNode().NextNodeId);
	for (auto meshNode : getMeshNodes())
	{
		auto transform = meshNode->getTransform();
		for (auto meshId : meshNode->Meshes)
		{
			nodes.emplace_back(SingleMeshSceneNode{
				meshNode->NodeId,
				meshId,
				meshNode,
				transform
			});
		}
	}
	return nodes;
}

AABB Scene::getSceneAABB()
{
	AABB aabb;
	for (const auto &s : sceneGraph.getFlattenedMeshNodes())
	{
		const auto& otherAABB = getMeshIndexedSpan(s.MeshId).AxisAlignedBB;
		aabb.contain(otherAABB.transform(s.ComputedTransform));
	}
	return aabb;
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

const Vector SceneNode::getCentrePosition() const
{
	Vector accum = InitialCentrePosition;
	auto mySize = Children.size() + Meshes.size();
	if (mySize == 0)
		return accum;
	for (auto meshId : Meshes)
		accum = DirectX::XMVectorAdd(accum, Scene.getMeshIndexedSpan(meshId).CentrePosition);
	for (auto& snChild : Children)
		accum = DirectX::XMVectorAdd(accum, DirectX::XMVector3Transform(snChild->getCentrePosition(), snChild->Transform)); // Do we need to transform child CentrePosition?
	const float invSize = 1.f / mySize;
	return DirectX::XMVectorMultiply(accum, DirectX::XMVectorSet(invSize, invSize, invSize, invSize));
}

void SceneNode::transform(const Matrix& trans)
{
	Transform = trans;
}

std::vector<AnimationRecord>& candela::scene::Scene::getAnimationRecords()
{
	return animationRecords;
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
const vector<Scene::LightNode> Scene::getNonDirectionalExternalLights() const
{
	auto ret = externalLights | std::ranges::views::filter([](const Scene::LightNode& ln) {
		return ln.Light.Type != LT_DIRECTIONAL;
	});
	return vector<Scene::LightNode>(ret.begin(), ret.end());
}
const vector<Scene::LightNode>& Scene::getExternalLights() const { return externalLights; }
const vector<SpecularPrimitive>& Scene::getSpeculars() const { return speculars; }
const vector<FaceAttributes>& Scene::getFaceAttributes() const { return faceAttributes; }

const IndexedSpan& Scene::getMeshIndexedSpan(std::size_t meshId) const
{
	return meshes.at(meshId);
}

const vector<IndexedSpan>& Scene::getMeshIndexedSpanData() const
{
	return meshes;
}

const SceneNode& Scene::getSceneGraph() const { return sceneGraph; }
SceneNode& Scene::getSceneGraph() { return sceneGraph; }

void Scene::addCamera(CameraNode camera)
{
	cameras.push_back(camera);
}

const vector<Scene::CameraNode>& Scene::getCameras() const
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