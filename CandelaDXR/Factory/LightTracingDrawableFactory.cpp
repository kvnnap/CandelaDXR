#include "LightTracingDrawableFactory.h"
#include "VectorFactory.h"

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
using candela::mathematics::UVector2;
using candela::mathematics::factory::UVector2Factory;

unique_ptr<IDrawable> LightTracingDrawableFactory::create() const
{
	return unique_ptr<LightTracingShading>();
}

unique_ptr<IDrawable> LightTracingDrawableFactory::create(const ConfigurationNode& config) const
{
	const auto &confObject = config.asObject();
	auto instance = confObject.keyExists("Sampler")
		? UniformSamplerFactory().create(confObject["Sampler"])
		: UniformSamplerFactory().create();
	auto lightSamples = UVector2();
	if (confObject.keyExists("LightSamples"))
		lightSamples = *UVector2Factory().create(confObject["LightSamples"]);
	return make_unique<LightTracingShading>(move(instance), lightSamples);
}
