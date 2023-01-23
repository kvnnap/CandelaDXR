#pragma once

#include "feanor/core/factory/factory.h"
#include "Animation/IAnimation.h"

namespace candela::animation::factory
{
    class AnimationFactory
        : public feanor::factory::Factory<IAnimation>
    {
    public:
        std::unique_ptr<IAnimation> create() const override;
        std::unique_ptr<IAnimation> create(const feanor::configuration::ConfigurationNode& config) const override;
    };
}
