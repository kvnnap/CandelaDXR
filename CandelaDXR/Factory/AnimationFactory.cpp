#include "AnimationFactory.h"
#include "Animation/Animation.h"
#include "Environment/Environment.h"
#include "MathematicsFactory.h"

#include <unordered_map>

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::environment::Environment;
using candela::animation::Animation;
using candela::animation::IAnimation;
using candela::animation::MeshState;
using candela::animation::Transition;
using candela::animation::factory::AnimationFactory;

using candela::mathematics::factory::TransformComponentsFactory;

unique_ptr<IAnimation> AnimationFactory::create() const
{
    return make_unique<Animation>();
}

unique_ptr<IAnimation> AnimationFactory::create(const ConfigurationNode& config) const
{
    auto animation = make_unique<Animation>();

    std::unordered_map<std::string, std::size_t> stateMap;
    
    std::size_t i{};
    // Load states
    for (const auto& state : config["States"].asList())
    {
        MeshState meshState{};
        
        const auto& stateObject = state.asObject();
        if (stateObject.keyExists("Name"))
            stateMap[stateObject["Name"].read<std::string>()] = i;
        meshState = *TransformComponentsFactory().create(state);
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
    bool transAbs = false;
    if (config.asObject().keyExists("TranslationAbsolute"))
        transAbs = config["TranslationAbsolute"].read<bool>();

    animation->setInitialMeshStateId(
        initialMeshStateIdConf.is<std::string>() ? 
        stateMap.at(initialMeshStateIdConf.read<std::string>()) : 
        initialMeshStateIdConf.read<std::uint64_t>());
    animation->setTranslationAbsolute(transAbs);

    return animation;
}
