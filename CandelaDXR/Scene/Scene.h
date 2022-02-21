#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <map>
#include <unordered_map>

#include "Texture.h"
#include "Mathematics/Types.h"

namespace candela::scene
{
	struct SceneNode
	{
		// Connectivity
		SceneNode *Parent = nullptr;
		std::vector<SceneNode> Children;

		// Data
		mathematics::Matrix Transform;
		std::string NodeName;
		std::string GroupName;

		//SceneNode();
		void addChild(const std::string& nodeName, const std::string& groupName);
	};

	struct IndexedSpan
	{
		std::string Name;
		std::size_t Start;
		std::size_t Size;
	};

	struct alignas(16) FaceAttributes
	{
		std::uint32_t MaterialId;
		std::uint32_t AreaLightId;
		std::uint32_t InstanceIndex;
	};

	struct alignas(16) AreaLight {
		DirectX::XMVECTOR Intensity;
		std::uint32_t InstanceIndex;
		std::uint32_t PrimitiveId;
		std::uint32_t MaterialId;
	};

	struct alignas(16) Material
	{
		mathematics::Vector3 Diffuse;
		std::int32_t DiffuseTextureId;
		mathematics::Vector3 Emissive;
		std::int32_t EmissiveTextureId;
		mathematics::Vector3 Specular;
		std::int32_t SpecularTextureId;
		mathematics::Vector3 TransmissiveFilter;
		float RefractiveIndex;
		
		bool isEmissive();
	};

	class Scene
	{
	public:
		Scene();

		std::size_t addTexture(Texture texture);
		void addMaterial(Material texture);

		void startGroup(const std::string& name);
		void endGroup();
		void addFace(const std::array<mathematics::Vector3, 3> &pos,
					 const std::array<mathematics::Vector2, 3> &tex, 
					 const std::array<mathematics::Vector3, 3> &norm, 
					 std::uint32_t materialId);

		// Scene graph
		void addSceneNodeToGroupMapping(const std::string& sceneNodeName, const std::string& groupName);

		// Getters
		const std::vector<mathematics::Vector3>& getVertices() const;
		const std::vector<mathematics::Vector2>& getTextureCoords() const;
		const std::vector<mathematics::Vector3>& getNormals() const;
		const std::vector<int>& getIndices() const;

		const std::vector<Texture>& getTextures() const;
		const std::vector<Material>& getMaterials() const;
		const std::vector<AreaLight>& getLights() const;
		const std::vector<FaceAttributes>& getFaceAttributes() const;

		const IndexedSpan& getMeshIndexedSpan(const std::string& groupName) const;
		const std::unordered_map<std::string, IndexedSpan>& getMeshIndexedSpanDataMap() const;

		const SceneNode& getSceneGraph() const;
		SceneNode& getSceneGraph();

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
		std::vector<Texture> textures;
		std::vector<Material> materials;
		std::vector<AreaLight> lights;

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
		std::unordered_map<std::string, IndexedSpan> spanDataMap;

		// Faces
		std::vector<FaceAttributes> faceAttributes;

		// Groups for the index data - must connect with scene graph
		SceneNode sceneGraph;

		// Temp data
		std::string currentGroupName;
	};
}
