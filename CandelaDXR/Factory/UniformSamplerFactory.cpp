#include "UniformSamplerFactory.h"

#include "Sampler/UniformSampler.h"

using feanor::configuration::ConfigurationNode;
using candela::sampler::factory::UniformSamplerFactory;
using candela::sampler::ISampler;
using candela::sampler::UniformSampler;
using std::unique_ptr;
using std::make_unique;
using std::uint32_t;

unique_ptr<ISampler> UniformSamplerFactory::create() const
{
    return make_unique<UniformSampler>();
}

unique_ptr<ISampler> UniformSamplerFactory::create(const ConfigurationNode& config) const
{
    auto& configObject = config.asObject();
    return configObject.keyExists("Seed") ?
        make_unique<UniformSampler>(config["Seed"].read<uint32_t>()) :
        create();
}
