#include "PathTracingDrawableFactory.h"

#include "Renderer/PathTracingShading.h"

#include "UniformSamplerFactory.h"

using std::move;
using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::renderer::IDrawable;
using candela::renderer::PathTracingShading;
using candela::renderer::factory::PathTracingDrawableFactory;

using candela::sampler::factory::UniformSamplerFactory;

unique_ptr<IDrawable> PathTracingDrawableFactory::create() const
{
	return unique_ptr<PathTracingShading>();
}

unique_ptr<IDrawable> PathTracingDrawableFactory::create(const ConfigurationNode& config) const
{
	const auto& confObject = config.asObject();
	auto instance = confObject.keyExists("Sampler")
		? UniformSamplerFactory().create(confObject["Sampler"])
		: UniformSamplerFactory().create();
	auto specularOnly = confObject.keyExists("SpecularOnly")
		? confObject["SpecularOnly"].read<bool>()
		: false;
	auto pt = make_unique<PathTracingShading>(move(instance), specularOnly);
	pt->setName(confObject["Name"].read<std::string>());
	return pt;
}
