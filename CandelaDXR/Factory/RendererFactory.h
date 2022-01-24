#pragma once

#include "feanor/core/factory/factory.h"
#include "Renderer/IRenderer.h"

namespace candela::environment
{
    class Environment;
}

namespace candela::renderer::factory
{
    class RendererFactory
        : public feanor::factory::Factory<IRenderer>
    {
    public:
        RendererFactory(candela::environment::Environment& env);
        std::unique_ptr<IRenderer> create() const override;
        std::unique_ptr<IRenderer> create(const feanor::configuration::ConfigurationNode& config) const override;

    private:
        candela::environment::Environment& env;
    };
}
