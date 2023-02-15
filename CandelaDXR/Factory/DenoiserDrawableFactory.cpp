#include "DenoiserDrawableFactory.h"

#include "Renderer/DenoiserShading.h"

#include "UniformSamplerFactory.h"

using std::move;
using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::renderer::IDrawable;
using candela::renderer::LightTracingShading;
using candela::renderer::factory::DenoiserDrawableFactory;

using candela::sampler::factory::UniformSamplerFactory;
using candela::mathematics::UVector2;

unique_ptr<IDrawable> DenoiserDrawableFactory::create() const
{
	return make_unique<DenoiserShading>();
}

unique_ptr<IDrawable> DenoiserDrawableFactory::create(const ConfigurationNode& config) const
{
	auto den = make_unique<DenoiserShading>();
	den->setName(config["Name"].read<std::string>());
	return den;
}
