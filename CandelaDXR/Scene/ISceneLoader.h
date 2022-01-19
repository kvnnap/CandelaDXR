#pragma once

namespace candela::scene
{
    class ISceneLoader
    {
    public:
        virtual ~ISceneLoader() = default;

        // Loads all vertices, materials, textures in Scene Memory
        // This method appends all data to the scene, in order to allow multiple 
        // scene loaders to act on one scene object.
        virtual void loadScene() = 0;
    };
}
