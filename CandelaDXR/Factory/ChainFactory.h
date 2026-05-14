#pragma once

#include "feanor/core/factory/factory.h"
#include "Chain/IChain.h"

#include <vector>

namespace candela::environment
{
    class Environment;
}

namespace candela::chain::factory
{
    class ChainFactory
        : public feanor::factory::Factory<CFList>
    {
    public:
        ChainFactory(candela::environment::Environment& env);
        std::unique_ptr<CFList> create() const override;
        std::unique_ptr<CFList> create(const feanor::configuration::ConfigurationNode& config) const override;
    private:
        candela::environment::Environment& env;
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

    class ExposureChainFactory
        : public feanor::factory::Factory<chain::IChain>
    {
    public:
        std::unique_ptr<chain::IChain> create() const override;
        std::unique_ptr<chain::IChain> create(const feanor::configuration::ConfigurationNode& config) const override;
    };
}