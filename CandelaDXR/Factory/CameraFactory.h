#pragma once

#include "feanor/core/factory/factory.h"
#include "Renderer/Camera.h"

namespace candela::renderer::factory
{
    class CameraFactory
        : public feanor::factory::Factory<Camera>
    {
    public:
        std::unique_ptr<Camera> create() const override;
        std::unique_ptr<Camera> create(const feanor::configuration::ConfigurationNode& config) const override;
    };
}