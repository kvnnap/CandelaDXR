#include <vector>
#include <stdexcept>

#include "tiny_obj_loader.h"


#include "WavefrontSceneLoader.h"

using std::string;
using std::vector;
using std::runtime_error;

using tinyobj::attrib_t;
using tinyobj::material_t;
using tinyobj::shape_t;
using tinyobj::LoadObj;

using candela::scene::WavefrontSceneLoader;
using candela::mathematics::Vector4;

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
    int diffTexId = scene->getTextures().size();
    for (const auto& tinyMat : materials)
    {
        int currentDiffTexId = tinyMat.diffuse_texname.length() ? diffTexId++ : -1;
        if (tinyMat.diffuse_texname.empty())
            currentDiffTexId = -1;
        else
            scene->addTexture(tinyMat.diffuse_texname);
        
        scene->addMaterial(Material{
            Vector4(tinyMat.diffuse[0], tinyMat.diffuse[1], tinyMat.diffuse[2], 1.f),
            Vector4(tinyMat.emission[0], tinyMat.emission[1], tinyMat.emission[2], 0.f),
            diffTexId,
            diffTexId
        });
    }
}

void WavefrontSceneLoader::setFilePath(const string& filePath)
{
	this->filePath = filePath;
}
