#pragma once

#include "feanor/core/factory/factory.h"
#include "Scene/Scene.h"

namespace candela::environment
{
    class Environment;
}

namespace candela::scene::factory
{
    class SceneFactory
        : public feanor::factory::Factory<Scene>
    {
    public:
        SceneFactory(candela::environment::Environment& env);
        std::unique_ptr<Scene> create() const override;
        std::unique_ptr<Scene> create(const feanor::configuration::ConfigurationNode& config) const override;

    private:
        candela::environment::Environment& env;
    };
}
