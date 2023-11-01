#pragma once

#include "feanor/core/factory/factory.h"
#include "Mathematics/TransformComponents.h"

namespace candela::mathematics::factory
{
    class TransformComponentsFactory
        : public feanor::factory::Factory<TransformComponents>
    {
    public:
        std::unique_ptr<TransformComponents> create() const override;
        std::unique_ptr<TransformComponents> create(const feanor::configuration::ConfigurationNode& config) const override;
    };
}