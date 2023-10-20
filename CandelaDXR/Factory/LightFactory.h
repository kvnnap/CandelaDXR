#pragma once

#include "feanor/core/factory/factory.h"
#include "Scene/Scene.h"

namespace candela::scene::factory
{
    class LightFactory
        : public feanor::factory::Factory<Light>
    {
    public:
        std::unique_ptr<Light> create() const override;
        std::unique_ptr<Light> create(const feanor::configuration::ConfigurationNode& config) const override;
    };
}