#pragma once

#include "feanor/core/factory/factory.h"
#include "Chain/IChain.h"

#include <vector>

namespace candela::chain::factory
{
    class ChainFactory
        : public feanor::factory::Factory<CFList>
    {
    public:
        std::unique_ptr<CFList> create() const override;
        std::unique_ptr<CFList> create(const feanor::configuration::ConfigurationNode& config) const override;
    };

    class AlphaCorrectionChainFactory
        : public feanor::factory::Factory<chain::IChain>
    {
    public:
        std::unique_ptr<chain::IChain> create() const override;
        std::unique_ptr<chain::IChain> create(const feanor::configuration::ConfigurationNode& config) const override;
    };

    class FileOutputChainFactory
        : public feanor::factory::Factory<chain::IChain>
    {
    public:
        std::unique_ptr<chain::IChain> create() const override;
        std::unique_ptr<chain::IChain> create(const feanor::configuration::ConfigurationNode& config) const override;
    };

    class ToneMappingChainFactory
        : public feanor::factory::Factory<chain::IChain>
    {
    public:
        std::unique_ptr<chain::IChain> create() const override;
        std::unique_ptr<chain::IChain> create(const feanor::configuration::ConfigurationNode& config) const override;
    };
}