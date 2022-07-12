#include "AnimationFactory.h"
#include "Environment/Environment.h"
#include "VectorFactory.h"

#include <unordered_map>

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::environment::Environment;
using candela::animation::Animation;
using candela::animation::MeshState;
using candela::animation::Transition;
using candela::animation::factory::AnimationFactory;

using candela::mathematics::factory::Vector3Factory;

unique_ptr<Animation> AnimationFactory::create() const
{
    return make_unique<Animation>();
}

unique_ptr<Animation> AnimationFactory::create(const ConfigurationNode& config) const
{
    auto animation = create();

    animation->setMeshName(config["MeshName"].read<std::string>());

    std::unordered_map<std::string, std::size_t> stateMap;

    std::size_t i{};
    // Load states
    for (const auto& state : config["States"].asList())
    {
        MeshState meshState{};
        
        const auto& stateObject = state.asObject();
        if (stateObject.keyExists("Name"))
            stateMap[stateObject["Name"].read<std::string>()] = i;
        if (stateObject.keyExists("Translation"))
            meshState.Translate = DirectX::XMLoadFloat3(&*Vector3Factory().create(stateObject["Translation"]));
        if (stateObject.keyExists("Scale"))
            meshState.Scale = DirectX::XMLoadFloat3(&*Vector3Factory().create(stateObject["Scale"]));
        else 
            meshState.Scale = DirectX::XMVectorSet(1.f, 1.f, 1.f, 0.f);
        if (stateObject.keyExists("Rotation"))
            meshState.Rotate = DirectX::XMLoadFloat3(&*Vector3Factory().create(stateObject["Rotation"]));

        animation->addMeshState(meshState);
        ++i;
    }

    // Load transitions
    for (const auto& transition : config["Transitions"].asList())
    {
        Transition trans{};
        const auto& transitionObject = transition.asObject();

        const auto& stateLiteral = transitionObject["State"].asLiteral();
        if (stateLiteral.is<std::string>())
            trans.MeshStateId = stateMap.at(stateLiteral.read<std::string>());
        else
            trans.MeshStateId = stateLiteral.read<std::uint64_t>();
        trans.TimeMS = transitionObject["Duration"].read<std::uint32_t>();

        animation->addTransition(trans);
    }

    // Initial mesh state
    const auto& initialMeshStateIdConf = config["InitialMeshState"].asLiteral();

    animation->setInitialMeshStateId(
        initialMeshStateIdConf.is<std::string>() ? 
        stateMap.at(initialMeshStateIdConf.read<std::string>()) : 
        initialMeshStateIdConf.read<std::uint64_t>());

    return animation;
}
