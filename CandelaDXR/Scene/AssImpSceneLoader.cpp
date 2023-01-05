#include "AssImpSceneLoader.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "Exception/Exception.h"

#include <stdexcept>
#include <array>
#include <filesystem>

using std::filesystem::path;
using std::array;
using candela::scene::AssImpSceneLoader;
using candela::mathematics::Vector2;
using candela::mathematics::Vector3;

static path getConcatPath(path base, path other)
{
	base /= other;
	return base;
}

AssImpSceneLoader::AssImpSceneLoader(Scene* scene)
	: scene(scene)
{
}

void AssImpSceneLoader::loadScene()
{
	Assimp::Importer importer;
	auto pScene = importer.ReadFile(filePath, 
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType |
		aiProcess_GenNormals
	);

	if (!pScene || !pScene->mRootNode || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
		throw std::runtime_error(importer.GetErrorString());

	uint32_t baseMaterialId = static_cast<uint32_t>(scene->getMaterials().size());
	path basePath = path(filePath).parent_path();
	auto objName = path(filePath).filename().string();
	auto& sceneNode = scene->getSceneGraph().addChild(objName, {}, {});

	// Load materials
	for (unsigned int i = 0; i < pScene->mNumMaterials; ++i)
	{
		auto mat = pScene->mMaterials[i];
		auto matName = mat->GetName().C_Str();

		aiColor3D diff;
		aiColor3D emis;
		aiColor3D spec;
		aiColor3D tf;
		float opacity = 1.f;
		float ior = 1.f;

		aiReturn ret;
		ret = mat->Get(AI_MATKEY_COLOR_DIFFUSE, diff);
		ret = mat->Get(AI_MATKEY_COLOR_EMISSIVE, emis);
		ret = mat->Get(AI_MATKEY_COLOR_SPECULAR, spec);
		ret = mat->Get(AI_MATKEY_COLOR_TRANSPARENT, tf);
		ret = mat->Get(AI_MATKEY_OPACITY, opacity);
		ret = mat->Get(AI_MATKEY_REFRACTI, ior);

		// Load textures
		int32_t currentDiffTexId = -1;
		int32_t currentSpecTexId = -1;
		if (mat->GetTextureCount(aiTextureType_DIFFUSE))
		{
			aiString diffTex;
			mat->GetTexture(aiTextureType_DIFFUSE, 0, &diffTex);
			currentDiffTexId = scene->addTexture(getConcatPath(basePath, diffTex.C_Str()).string());
		}

		if (mat->GetTextureCount(aiTextureType_SPECULAR))
		{
			aiString specTex;
			mat->GetTexture(aiTextureType_SPECULAR, 0, &specTex);
			currentSpecTexId = scene->addTexture(getConcatPath(basePath, specTex.C_Str()).string());
		}

		// Materials point to textures using the identifier
		scene->addMaterial(Material{
			.Diffuse = Vector3(diff.r, diff.g, diff.b),
			.DiffuseTextureId = currentDiffTexId,
			.Emissive = Vector3(emis.r, emis.g, emis.b),
			.EmissiveTextureId = -1,
			.Specular = Vector3(spec.r, spec.g, spec.b),
			.SpecularTextureId = currentSpecTexId,
			.TransmissiveFilter = Vector3(tf.r, tf.g, tf.b),
			.RefractiveIndex = ior,
			.Dissolve = opacity,
			.EmissiveType = 0
			}, matName);
	}

	// Loop through meshes
	for (unsigned int i = 0; i < pScene->mNumMeshes; ++i)
	{
		auto mesh = pScene->mMeshes[i];
		auto meshName = mesh->mName.C_Str();

		array<Vector3, 3> pos;
		array<Vector2, 3> tex{}; // Check if value-initialises to zero
		array<Vector3, 3> norm{};

		scene->startGroup(meshName);

		// Faces
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
		{
			const auto& face = mesh->mFaces[f];
			if (face.mNumIndices != 3)
				ThrowException("Not loading a triangle");

			// Face (Triangle) indices
			for (unsigned int ind = 0; ind < face.mNumIndices; ++ind)
			{
				auto index = face.mIndices[ind];
				const auto& pVert = mesh->mVertices[index];
				pos[ind] = Vector3(pVert.x, pVert.y, pVert.z);
				
				if (mesh->mTextureCoords[0])
				{
					const auto& tVert = mesh->mTextureCoords[0][index];
					tex[ind] = Vector2(tVert.x, 1.f - tVert.y);
				}

				if (mesh->mNormals)
				{
					const auto& nVert = mesh->mNormals[index];
					norm[ind] = Vector3(nVert.x, nVert.y, nVert.z);
				}
			}

			scene->addFace(pos, tex, norm, baseMaterialId + static_cast<uint32_t>(mesh->mMaterialIndex));
		}

		// End this group
		scene->endGroup();

		// Add to scene graph
		scene->addSceneNodeToGroupMapping(sceneNode, meshName, meshName);
	}
}

void AssImpSceneLoader::setFilePath(const std::string& p_filePath)
{
	filePath = p_filePath;
}

void AssImpSceneLoader::setAlwaysComputeNormals(bool value)
{
	alwaysComputeNormals = value;
}

