#include "LightTracingDrawableFactory.h"
#include "VectorFactory.h"

#include "Renderer/LightTracingShading.h"
#include "Renderer/LTOptimisedComponent.h"
#include "Renderer/LTRasterGuidedShading.h"

#include "UniformSamplerFactory.h"

using std::move;
using std::unique_ptr;
using std::make_unique;
using std::uint32_t;

using feanor::configuration::ConfigurationNode;

using candela::renderer::IDrawable;
using candela::renderer::LightTracingShading;
using candela::renderer::LTOptimisedComponent;
using candela::renderer::LTRasterGuidedShading;
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

	auto samplerPt = instance.get();
	auto lt = make_unique<LightTracingShading>(move(instance), lightSamples);
	lt->setName(confObject["Name"].read<std::string>());

	{
		bool load = true;
		if (confObject.keyExists("LightTracingShader"))
		{
			const auto& childConfig = confObject["LightTracingShader"].asObject();
			if (childConfig.keyExists("Load"))
				load = childConfig["Load"].read<bool>();
		}
		if (load)
			lt->addLtShader("./Shaders/LightTracingShader.cso", unique_ptr<ILightTracingComponent>());
	}

	{
		bool load = true;
		float causticsRatio = 0.5f;

		if (confObject.keyExists("LightTracingOptimisedShader"))
		{
			const auto& childConfig = confObject["LightTracingOptimisedShader"].asObject();
			if (childConfig.keyExists("Load"))
				load = childConfig["Load"].read<bool>();
			if (childConfig.keyExists("CausticsRatio"))
				causticsRatio = childConfig["CausticsRatio"].read<float>();
		}
		
		if (load)
		{
			auto ltOptShad = make_unique<LTOptimisedComponent>();
			ltOptShad->setCausticsRatio(causticsRatio);
			lt->addLtShader("./Shaders/LightTracingOptimisedShader.cso", move(ltOptShad));
		}
		
	}

	{
		bool load = true;
		uint32_t metricMode = 0;
		uint32_t filterSize = 17;

		if (confObject.keyExists("LightTracingImportanceShader"))
		{
			const auto& childConfig = confObject["LightTracingImportanceShader"].asObject();
			if (childConfig.keyExists("Load"))
				load = childConfig["Load"].read<bool>();
			if (childConfig.keyExists("Metric"))
				metricMode = childConfig["Metric"].read<uint32_t>();
			if (childConfig.keyExists("FilterSize"))
				filterSize = childConfig["FilterSize"].read<uint32_t>();
		}

		if (load)
		{
			auto ltRastShad = make_unique<LTRasterGuidedShading>(samplerPt, true);
			ltRastShad->setDistanceMetricMode(metricMode);
			ltRastShad->setFilterSize(filterSize);
			lt->addLtShader("./Shaders/LightTracingImportanceShader.cso", move(ltRastShad));
		}
	}

	if (confObject.keyExists("ActiveShaderIndex"))
		lt->setCurrentShaderIndex(confObject["ActiveShaderIndex"].read<std::uint32_t>());
	if (confObject.keyExists("SeparateCaustics"))
		lt->seperateCaustics(confObject["SeparateCaustics"].read<bool>() ? 1u : 0u);
	if (confObject.keyExists("CausticsBlurSize"))
		lt->setCausticsBlurSize(confObject["CausticsBlurSize"].read<std::uint32_t>());
	return lt;
}
