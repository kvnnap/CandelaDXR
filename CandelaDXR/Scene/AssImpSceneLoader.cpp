#include "AssImpSceneLoader.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "Exception/Exception.h"

#include <stdexcept>
#include <array>
#include <filesystem>
#include <math.h>

using std::filesystem::path;
using std::array;
using std::string;
using std::make_unique;
using candela::scene::AssImpSceneLoader;
using candela::scene::SceneNode;
using candela::scene::Scene;
using candela::scene::AssImpOffsets;
using candela::scene::Texture;
using candela::scene::MemoryTexture;
using candela::scene::StbTexture;
using candela::mathematics::Vector2;
using candela::mathematics::Vector3;
using candela::renderer::Camera;

static path getConcatPath(path base, path other)
{
	base /= other;
	return base;
}

static void processSceneGraph(Scene& scene, const AssImpOffsets& offsets, const aiScene* sceneAi, SceneNode* sceneNode, const aiNode* sceneAiNode)
{
	string nodeName = sceneAiNode->mName.C_Str();
	string groupName;

	// Process current node
	auto& childSceneNode = sceneNode->addChild(nodeName);

	// Add Meshes
	for (unsigned int i = 0; i < sceneAiNode->mNumMeshes; ++i)
		childSceneNode.Meshes.push_back(offsets.mesh + sceneAiNode->mMeshes[i]);

	memcpy(&childSceneNode.Transform, &sceneAiNode->mTransformation, sizeof(childSceneNode.Transform));
	childSceneNode.Transform = DirectX::XMMatrixTranspose(childSceneNode.Transform); // assimp mat should have been row-major, why tranpose?
	// Process scene graph
	for (unsigned int i = 0; i < sceneAiNode->mNumChildren; ++i)
		processSceneGraph(scene, offsets, sceneAi, &childSceneNode, sceneAiNode->mChildren[i]);

	childSceneNode.processCentrePositionsForDirectChildren();
}

static int32_t addTextureToScene(Scene* scene, path basePath, const aiScene* pScene, aiString texFileName)
{
	int32_t currentTexId = -1;
	auto emDiffTex = pScene->GetEmbeddedTexture(texFileName.C_Str());
	if (emDiffTex)
	{
		if (emDiffTex->mHeight == 0)
			currentTexId = static_cast<int32_t>(scene->addTexture(make_unique<StbTexture>(emDiffTex->pcData, emDiffTex->mWidth)));
		else
			currentTexId = static_cast<int32_t>(scene->addTexture(make_unique<MemoryTexture>(emDiffTex->pcData, emDiffTex->mWidth, emDiffTex->mHeight)));
	}
	else
	{
		currentTexId = static_cast<int32_t>(scene->addTexture(make_unique<StbTexture>(getConcatPath(basePath, texFileName.C_Str()).string())));
	}

	return currentTexId;
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
	const auto objName = path(filePath).filename().string();

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
			currentDiffTexId = addTextureToScene(scene, basePath, pScene, diffTex);
		}

		if (mat->GetTextureCount(aiTextureType_SPECULAR))
		{
			aiString specTex;
			mat->GetTexture(aiTextureType_SPECULAR, 0, &specTex);
			currentSpecTexId = addTextureToScene(scene, basePath, pScene, specTex);
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
	offsets.mesh = scene->getMeshIndexedSpanData().size();
	for (unsigned int i = 0; i < pScene->mNumMeshes; ++i)
	{
		auto mesh = pScene->mMeshes[i];
		auto meshName = mesh->mName.C_Str();

		array<Vector3, 3> pos;
		array<Vector2, 3> tex{}; // Check if value-initialises to zero
		array<Vector3, 3> norm{};

		scene->startMesh(meshName);

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
		scene->endMesh();
	}

	processSceneGraph(*scene, offsets, pScene, &scene->getSceneGraph(), pScene->mRootNode);

	auto mySceneNodes = scene->getSceneGraph().getAllNodes();

	// Add cameras
	offsets.camera = scene->getCameras().size();
	for (unsigned int i = 0; i < pScene->mNumCameras; ++i)
	{
		const auto camera = pScene->mCameras[i];

		// Construct camera
		auto nearWidth = 2.f * camera->mClipPlaneNear * std::tan(camera->mHorizontalFOV * 0.5f);
		auto nearHeight = nearWidth / camera->mAspect;
		Camera myCamera = Camera(
			DirectX::XMVectorSet(camera->mPosition.x, camera->mPosition.y, camera->mPosition.z, 1.f),
			DirectX::XMVectorSet(camera->mLookAt.x, camera->mLookAt.y, camera->mLookAt.z, 0.f),
			nearWidth, nearHeight,
			camera->mClipPlaneNear, camera->mClipPlaneFar,
			DirectX::XMVectorSet(camera->mUp.x, camera->mUp.y, camera->mUp.z, 0.f)
		);
		myCamera.setName(camera->mName.C_Str());

		// Transform according to scene graph
		auto myCameraNode = *std::find_if(mySceneNodes.begin(), mySceneNodes.end(), [&myCamera](const SceneNode* n) -> bool {
			return n->NodeName == myCamera.getName();
		});

		myCamera.transform(myCameraNode->getTransform());

		// Add to scene
		scene->addCamera({ myCamera, myCameraNode});
	}

	scene->recalculateLightsAndFaceAttributes();

}

void AssImpSceneLoader::setFilePath(const std::string& p_filePath)
{
	filePath = p_filePath;
}

void AssImpSceneLoader::setAlwaysComputeNormals(bool value)
{
	alwaysComputeNormals = value;
}

