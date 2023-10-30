#pragma once

#include "feanor/core/factory/factory.h"
#include "Scene/ISceneModifier.h"

namespace candela::environment
{
    class Environment;
}

namespace candela::scene::factory
{
    class SceneModifierFactory
        : public feanor::factory::Factory<ISceneModifier>
    {
    public:
        SceneModifierFactory(candela::environment::Environment& env);
        std::unique_ptr<ISceneModifier> create() const override;
        std::unique_ptr<ISceneModifier> create(const feanor::configuration::ConfigurationNode& config) const override;

    private:
        candela::environment::Environment& env;
    };
}
