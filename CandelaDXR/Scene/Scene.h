#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <map>
#include <unordered_map>

#include "Texture.h"
#include "Material.h"
#include "Mathematics/Types.h"

namespace candela::scene
{
	struct SceneNode
	{
		// Connectivity
		std::unique_ptr<SceneNode> Parent;
		std::vector<std::unique_ptr<SceneNode>> Children;

		// Data
		mathematics::Matrix Transform;
		std::string NodeName;
		std::string GroupName;
	};

	struct IndexedSpan
	{
		std::string Name;
		std::size_t Start;
		std::size_t Size;
	};

	struct alignas(16) FaceAttributes
	{
		std::uint32_t materialId;
		std::uint32_t areaLightId;
	};

	struct alignas(16) AreaLight {
		DirectX::XMVECTOR intensity;
		std::uint32_t instanceIndex;
		std::uint32_t primitiveId;
		std::uint32_t materialId;
	};

	class Scene
	{
	public:
		Scene();

		const std::vector<Texture>& getTextures() const;
		std::size_t addTexture(Texture texture);

		const std::vector<Material>& getMaterials() const;
		void addMaterial(Material texture);

		void startGroup(const std::string& name);
		void endGroup();
		void addFace(const std::array<mathematics::Vector3, 3> &pos,
					 const std::array<mathematics::Vector2, 3> &tex, 
					 const std::array<mathematics::Vector3, 3> &norm, 
					 std::uint32_t materialId);
	private:
		// Methods
		//void addVertex(mathematics::Vector3 pos, mathematics::Vector2 tex, mathematics::Vector3 normal);

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

		// Group data - Divides index data into sections that make up the meshes
		std::unordered_map<std::string, IndexedSpan> spanDataMap;

		std::map<std::array<float, 8>, int> collisionMap;

		// Groups for the index data - must connect with scene graph
		SceneNode sceneGraph;

		// Temp data
		std::string currentGroupName;
	};
}
