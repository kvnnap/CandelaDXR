#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <map>
#include <memory>

#include "Scene/Texture/Texture.h"
#include "Mathematics/Types.h"
#include "Mathematics/AABB.h"
#include "Renderer/ITransform.h"
#include "Renderer/Camera.h"
#include "Animation/IAnimation.h"

#include "Shader/Scene.hlsli"

namespace candela::scene
{
	struct SceneNode;
	class Scene;

	struct SingleMeshSceneNode
	{
		std::size_t NodeId;
		std::size_t MeshId;
		SceneNode* SceneNode;
		mathematics::Matrix ComputedTransform;
	};

	struct SceneNode
		: public renderer::ITransform
	{
		// Connectivity
		SceneNode(Scene& scene, SceneNode* parent = nullptr);

		// Data
		Scene& Scene;
		SceneNode* Parent{};
		std::vector<std::unique_ptr<SceneNode>> Children;
		std::size_t NextNodeId{};
		mathematics::Matrix Transform;
		std::size_t NodeId{};
		std::string NodeName;
		std::vector<std::size_t> Meshes;
		std::vector<std::size_t> Cameras;
		std::vector<std::size_t> Lights;
		mathematics::Vector InitialCentrePosition{};

		// Methods
		std::size_t assignNewNodeId();
		SceneNode& getRootNode();
		SceneNode* getNode(const std::string& nodeName);

		//SceneNode();
		SceneNode& addChild(const std::string& sceneNodeName);
		bool isLeaf() const;
		mathematics::Matrix getTransform() const;

		void getMeshNodes(std::vector<SceneNode*>& meshNodes);
		std::vector<SceneNode*> getMeshNodes();
		std::vector<SingleMeshSceneNode> getFlattenedMeshNodes();

		void getAllNodes(std::vector<SceneNode*>& nodes);
		std::vector<SceneNode*> getAllNodes();

		const mathematics::Vector getCentrePosition() const override;
		void transform(const mathematics::Matrix& trans) override;
	};

	struct IndexedSpan
	{
		std::string Name;
		std::size_t Start;
		std::size_t Size;
		DirectX::XMVECTOR CentrePosition;
		mathematics::AABB AxisAlignedBB;
	};

	//struct ObjectNode
	//{
	//	renderer::ITransform* Object{}; // Camera or Light or nullptr
	//	renderer::ITransform* Node{}; // SceneNode
	//};

	struct AnimationPair // Channel in Assimp lingo
	{
		animation::IAnimation* Animation{};
		std::vector<renderer::ITransform*> Node; // Nodes affected by this Animation
	};

	struct AnimationRecord
	{
		std::vector<AnimationPair> AnimPair;
		std::string Name;
		bool Enabled = true;
	};


	class Scene
	{
	public:
		Scene();

		std::size_t addTexture(std::unique_ptr<Texture> texture);
		void addMaterial(Material material, const std::string& name = "");

		void startMesh(const std::string& meshName);
		std::size_t endMesh();
		void addFace(const std::array<mathematics::Vector3, 3>& pos,
			const std::array<mathematics::Vector2, 3>& tex,
			const std::array<mathematics::Vector3, 3>& norm,
			std::uint32_t materialId);
		void recalculateLightsAndFaceAttributes();
		void addAnimation(std::unique_ptr<animation::IAnimation> animation);
		AnimationRecord& addAnimationRecord();

		// Getters
		std::vector<AnimationRecord>& getAnimationRecords();

		const std::vector<mathematics::Vector3>& getVertices() const;
		const std::vector<mathematics::Vector2>& getTextureCoords() const;
		const std::vector<mathematics::Vector3>& getNormals() const;
		const std::vector<int>& getIndices() const;

		const std::vector<std::unique_ptr<Texture>>& getTextures() const;
		std::string getMaterialName(std::size_t matId) const;
		const std::vector<Material>& getMaterials() const;
		std::vector<Material>& getMaterials();
		const std::vector<AreaLight>& getLights() const;
		const std::vector<SpecularPrimitive>& getSpeculars() const;
		const std::vector<FaceAttributes>& getFaceAttributes() const;

		const IndexedSpan& getMeshIndexedSpan(std::size_t meshId) const;
		const std::vector<IndexedSpan>& getMeshIndexedSpanData() const;

		const SceneNode& getSceneGraph() const;
		SceneNode& getSceneGraph();
		mathematics::AABB getSceneAABB();

		struct CameraNode
		{
			renderer::Camera Camera;
			SceneNode* Node;
		};
		void addCamera(CameraNode camera);
		const std::vector<CameraNode>& getCameras() const;

		struct LightNode
		{
			Light Light;
			SceneNode* Node;
		};

		const std::vector<LightNode> getNonDirectionalExternalLights() const;
		const std::vector<LightNode>& getExternalLights() const;
		void addExternalLight(const LightNode& light);

		// Utility functions - Offsets in bytes
		const std::size_t getVerticesOffset() const;
		const std::size_t getVerticesSizeBytes() const;
		const std::size_t getTextureCoordsOffset() const;
		const std::size_t getTextureCoordsSizeBytes() const;
		const std::size_t getNormalsOffset() const;
		const std::size_t getNormalsSizeBytes() const;
		const std::size_t getIndicesOffset() const;
		const std::size_t getIndicesSizeBytes() const;

	private:
		// Data
		std::vector<std::unique_ptr<Texture>> textures;
		std::vector<Material> materials;
		std::vector<std::string> materialNames;
		std::vector<AreaLight> lights;
		std::vector<SpecularPrimitive> speculars;
		std::vector<CameraNode> cameras;
		std::vector<LightNode> externalLights;

		// These contain the vertices. The 3 arrays must all be the same size
		// Storing separately vs interleaved. Trying separate first.
		std::vector<mathematics::Vector3> vertices;
		std::vector<mathematics::Vector2> textureCoords;
		std::vector<mathematics::Vector3> normals;

		// Global index data - these indices refer to the above arrays
		std::vector<int> indexData;

		// Used to filter out duplicate triples <pos, tex, norm>
		std::map<std::array<float, 8>, int> collisionMap;

		// Group data - Divides index data into sections that make up the meshes
		std::vector<IndexedSpan> meshes;

		// Faces
		std::vector<FaceAttributes> faceAttributes;

		// Groups for the index data - must connect with scene graph
		SceneNode sceneGraph;

		// Animations
		std::vector<std::unique_ptr<animation::IAnimation>> animations;
		std::vector<AnimationRecord> animationRecords;

		// Temp data
		std::string currentGroupName;
		DirectX::XMVECTOR posAccumulator;
		mathematics::AABB aabbAccum;
	};
}
