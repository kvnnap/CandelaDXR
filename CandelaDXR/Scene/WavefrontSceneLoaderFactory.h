#pragma once

#include "feanor/core/factory/factory.h"
#include "Scene/ISceneLoader.h"

namespace candela::environment
{
    class Environment;
}

namespace candela::scene::factory
{
    class WavefrontSceneLoaderFactory
        : public feanor::factory::Factory<ISceneLoader>
    {
    public:
        WavefrontSceneLoaderFactory(candela::environment::Environment& env);
        std::unique_ptr<ISceneLoader> create() const override;
        std::unique_ptr<ISceneLoader> create(const feanor::configuration::ConfigurationNode& config) const override;

    private:
        candela::environment::Environment& env;
    };
}
