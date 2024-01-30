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
	LightTracingShading::LTComponents components = { true, true, true };
	if (confObject.keyExists("Components"))
	{
		const auto& compsConfig = confObject["Components"].asObject();
		if (compsConfig.keyExists("Normal")) 
			components.normal = compsConfig["Normal"].read<bool>();
		if (compsConfig.keyExists("Optimised"))
			components.optimised = compsConfig["Optimised"].read<bool>();
		if (compsConfig.keyExists("Importance"))
			components.importance = compsConfig["Importance"].read<bool>();
	}
	auto lt = make_unique<LightTracingShading>(move(instance), lightSamples, components);
	lt->setName(confObject["Name"].read<std::string>());
	if (confObject.keyExists("SeparateCaustics"))
		lt->seperateCaustics(confObject["SeparateCaustics"].read<bool>() ? 1u : 0u);
	if (confObject.keyExists("CausticsBlurSize"))
		lt->setCausticsBlurSize(confObject["CausticsBlurSize"].read<std::uint32_t>());
	return lt;
}
