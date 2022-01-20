#include <vector>
#include <stdexcept>
#include <cstdint>

#include "tiny_obj_loader.h"


#include "WavefrontSceneLoader.h"

using std::string;
using std::vector;
using std::runtime_error;
using std::int32_t;

using tinyobj::attrib_t;
using tinyobj::material_t;
using tinyobj::shape_t;
using tinyobj::LoadObj;

using candela::scene::WavefrontSceneLoader;
using candela::mathematics::Vector3;

void WavefrontSceneLoader::loadScene()
{
    const string noMaterialKey = "__nomat__";

    // Register instances in env
    attrib_t attr = {};
    vector<shape_t> shapes;
    vector<material_t> materials;
    string warn;
    string err;
    LoadObj(&attr, &shapes, &materials, &warn, &err, filePath.c_str());

    // If tinyobj fails, throw.
    if (!err.empty())
        throw runtime_error(err);

    // Load Materials
    for (const auto& tinyMat : materials)
    {
        int32_t currentDiffTexId = -1;
        if (!tinyMat.diffuse_texname.empty())
            currentDiffTexId = static_cast<int>(scene->addTexture(tinyMat.diffuse_texname));
        
        // Materials point to textures using the identifier
        scene->addMaterial(Material{
            Vector3(tinyMat.diffuse[0], tinyMat.diffuse[1], tinyMat.diffuse[2]),
            currentDiffTexId,
            Vector3(tinyMat.emission[0], tinyMat.emission[1], tinyMat.emission[2]),
            -1
        });
    }

    // Load objects
    attr.vertices;

    // Grouping
    for (const auto& shape : shapes)
    {
        
    }

}

void WavefrontSceneLoader::setFilePath(const string& filePath)
{
	this->filePath = filePath;
}
