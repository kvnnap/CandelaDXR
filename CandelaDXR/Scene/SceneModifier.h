#pragma once

#include "ISceneModifier.h"

#include "Scene.h"
#include "Mathematics/Types.h"
#include "Mathematics/TransformComponents.h"

#include <unordered_map>

namespace candela::scene
{

    template<class T>
    struct Property 
    {
        std::string Type; // Material, Light
        std::string Name; // Material with name Name, etc
        std::string PropertyName; // The property name of the material, light
        std::size_t Id; // Used if PropertyName is empty
        std::unordered_map<std::string, std::string> ExtraConfig;
        T Data; // The data to be set to that property
    };

    class SceneModifier
        : public ISceneModifier
    {
    public:

        SceneModifier(Scene* scene);
        
        // Applies modifiers to scene materials, lights, etc
        void modifyScene() override;

        void addProperty(const Property<mathematics::Vector3>& prop);
        void addProperty(const Property<mathematics::TransformComponents>& prop);
        void addProperty(const Property<float>& prop);

    private:
        Scene* scene{};

        std::vector<Property<mathematics::Vector3>> vec3Properties;
        std::vector<Property<mathematics::TransformComponents>> transComponentsProperties;
        std::vector<Property<float>> floatProperties;
    };
}
