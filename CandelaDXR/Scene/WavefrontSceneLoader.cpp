#include <vector>
#include <stdexcept>
#include <cstdint>

#include "tiny_obj_loader.h"

#include "Exception/Exception.h"
#include "WavefrontSceneLoader.h"

using std::string;
using std::vector;
using std::runtime_error;
using std::int32_t;
using std::size_t;
using std::array;

using tinyobj::attrib_t;
using tinyobj::material_t;
using tinyobj::shape_t;
using tinyobj::LoadObj;

using candela::mathematics::Vector2;
using candela::mathematics::Vector3;

using candela::scene::WavefrontSceneLoader;
using candela::mathematics::Vector3;

WavefrontSceneLoader::WavefrontSceneLoader(Scene* scene)
    : scene(scene)
{
}

void WavefrontSceneLoader::loadScene()
{
    const string noMaterialKey = "__nomat__";

    // Register instances in env
    attrib_t attr = {};
    vector<shape_t> shapes;
    vector<material_t> materials;
    string warn;
    string err;
    string baseDir;
    if (filePath.find_last_of("/\\") != std::string::npos)
        baseDir = filePath.substr(0, filePath.find_last_of("/\\"));

    LoadObj(&attr, &shapes, &materials, &warn, &err, filePath.c_str(), baseDir.c_str());

    // If tinyobj fails, throw.
    if (!err.empty())
        throw runtime_error(err);

    // Load Materials
    for (const auto& tinyMat : materials)
    {
        int32_t currentDiffTexId = -1;
        int32_t currentSpecTexId = -1;
        if (!tinyMat.diffuse_texname.empty())
            currentDiffTexId = static_cast<int>(scene->addTexture(tinyMat.diffuse_texname));
        if (!tinyMat.specular_texname.empty())
            currentSpecTexId = static_cast<int>(scene->addTexture(tinyMat.specular_texname));
        
        // Materials point to textures using the identifier
        scene->addMaterial(Material{
            .Diffuse = Vector3(tinyMat.diffuse[0], tinyMat.diffuse[1], tinyMat.diffuse[2]),
            .DiffuseTextureId = currentDiffTexId,
            .Emissive = Vector3(tinyMat.emission[0], tinyMat.emission[1], tinyMat.emission[2]),
            .EmissiveTextureId = -1,
            .Specular = Vector3(tinyMat.specular[0], tinyMat.specular[1], tinyMat.specular[2]),
            .SpecularTextureId = currentSpecTexId,
            .TransmissiveFilter = Vector3(tinyMat.transmittance[0], tinyMat.transmittance[1], tinyMat.transmittance[2]),
            .RefractiveIndex = tinyMat.ior,
            .Dissolve = tinyMat.dissolve
        });
    }

    // Grouping - for each mesh group
    for (const auto& shape : shapes)
    {
        size_t index = 0;
        size_t faceNum = 0;
        scene->startGroup(shape.name);

        // For each face - i.e every triangle
        for (const auto& vertexCountForFace : shape.mesh.num_face_vertices)
        {
            if (vertexCountForFace != 3)
                ThrowException("Not loading a triangle");

            array<Vector3, 3> pos;
            array<Vector2, 3> tex {}; // Check if value-initialises to zero
            array<Vector3, 3> norm;

            bool computingNormals = alwaysComputeNormals;

            // For each vertex in the face (vertex in tri)
            for (size_t v = 0; v < vertexCountForFace; ++v)
            {
                // Position
                auto vertexIndex = shape.mesh.indices[index + v].vertex_index;
                auto vL = 3 * static_cast<size_t>(vertexIndex); // Vertex Loc
                pos[v] = Vector3(attr.vertices[vL], attr.vertices[vL + 1], attr.vertices[vL + 2]);

                // Tex Coord
                auto texIndex = shape.mesh.indices[index + v].texcoord_index;
                if (texIndex != -1)
                {
                    auto tL = 2 * static_cast<size_t>(texIndex);
                    tex[v] = Vector2(attr.texcoords[tL], attr.texcoords[tL + 1]);
                }

                // Norm Coord
                auto normIndex = shape.mesh.indices[index + v].normal_index;
                computingNormals |= alwaysComputeNormals || normIndex == -1;
                if (!computingNormals)
                {
                    auto nL = 3 * static_cast<size_t>(normIndex); // Normal Loc
                    norm[v] = Vector3(attr.vertices[nL], attr.vertices[nL + 1], attr.vertices[nL + 2]);
                }
            }

            // Normals
            if (computingNormals)
            {
                using namespace DirectX;
                auto v0 = XMLoadFloat3(&pos[0]);
                XMStoreFloat3(&norm[0], XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&pos[1]) - v0, XMLoadFloat3(&pos[2]) - v0)));
                norm[1] = norm[2] = norm[0];
            }

            // Add face to the scene. Every face is made up of a material
            const auto materialId = shape.mesh.material_ids[faceNum];
            scene->addFace(pos, tex, norm, static_cast<uint32_t>(materialId));
            index += vertexCountForFace;
            ++faceNum;
        }

        // End this group
        scene->endGroup();

        // Add to scene graph
        scene->addSceneNodeToGroupMapping(shape.name, shape.name);
    }
}

void WavefrontSceneLoader::setFilePath(const string& filePath)
{
	this->filePath = filePath;
}

void WavefrontSceneLoader::setAlwaysComputeNormals(bool value)
{
    alwaysComputeNormals = value;
}
