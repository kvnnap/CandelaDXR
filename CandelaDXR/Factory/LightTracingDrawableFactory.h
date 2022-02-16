#pragma once

#include "feanor/core/factory/factory.h"
#include "Renderer/IDrawable.h"

namespace candela::renderer::factory
{
    class LightTracingDrawableFactory
        : public feanor::factory::Factory<IDrawable>
    {
    public:
        std::unique_ptr<IDrawable> create() const override;
        std::unique_ptr<IDrawable> create(const feanor::configuration::ConfigurationNode& config) const override;
    };
}
