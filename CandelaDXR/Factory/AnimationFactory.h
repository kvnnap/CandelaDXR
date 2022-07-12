#pragma once

#include "feanor/core/factory/factory.h"
#include "Animation/Animation.h"

namespace candela::animation::factory
{
    class AnimationFactory
        : public feanor::factory::Factory<Animation>
    {
    public:
        std::unique_ptr<Animation> create() const override;
        std::unique_ptr<Animation> create(const feanor::configuration::ConfigurationNode& config) const override;
    };
}
