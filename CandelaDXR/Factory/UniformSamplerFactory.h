#pragma once

#include "feanor/core/factory/factory.h"
#include "Sampler/ISampler.h"

namespace candela::sampler::factory
{
    class UniformSamplerFactory
        : public feanor::factory::Factory<ISampler>
    {
    public:
        std::unique_ptr<ISampler> create() const override;
        std::unique_ptr<ISampler> create(const feanor::configuration::ConfigurationNode& config) const override;
    };
}
