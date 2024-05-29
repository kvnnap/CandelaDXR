#include "OptixDenoiserDrawableFactory.h"

#include "Renderer/OptixDenoiserShading.h"

#include "UniformSamplerFactory.h"

using std::move;
using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::renderer::IDrawable;
using candela::renderer::factory::OptixDenoiserDrawableFactory;

using candela::sampler::factory::UniformSamplerFactory;
using candela::mathematics::UVector2;

unique_ptr<IDrawable> OptixDenoiserDrawableFactory::create() const
{
	return make_unique<OptixDenoiserShading>();
}

unique_ptr<IDrawable> OptixDenoiserDrawableFactory::create(const ConfigurationNode& config) const
{
	auto den = make_unique<OptixDenoiserShading>();
	den->setName(config["Name"].read<std::string>());
	return den;
}
