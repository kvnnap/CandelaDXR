#include "LightTracingDrawableFactory.h"

#include "Renderer/LightTracingShading.h"

#include "UniformSamplerFactory.h"

using std::move;
using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::renderer::IDrawable;
using candela::renderer::LightTracingShading;
using candela::renderer::factory::LightTracingDrawableFactory;

using candela::sampler::factory::UniformSamplerFactory;

unique_ptr<IDrawable> LightTracingDrawableFactory::create() const
{
	return unique_ptr<LightTracingShading>();
}

unique_ptr<IDrawable> LightTracingDrawableFactory::create(const ConfigurationNode& config) const
{
	auto instance = config.asObject().keyExists("Sampler")
		? UniformSamplerFactory().create(config["Sampler"])
		: UniformSamplerFactory().create();
	return make_unique<LightTracingShading>(move(instance));
}
