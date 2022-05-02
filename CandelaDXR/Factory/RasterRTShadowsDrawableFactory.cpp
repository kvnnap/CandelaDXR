#include "RasterRTShadowsDrawableFactory.h"

#include "Renderer/RasterRTShadowsShading.h"

#include "UniformSamplerFactory.h"

using std::unique_ptr;
using std::make_unique;
using std::move;

using feanor::configuration::ConfigurationNode;

using candela::sampler::factory::UniformSamplerFactory;

using candela::renderer::IDrawable;
using candela::renderer::RasterRTShadowsShading;
using candela::renderer::factory::RasterRTShadowsDrawableFactory;

unique_ptr<IDrawable> RasterRTShadowsDrawableFactory::create() const
{
	return unique_ptr<RasterRTShadowsShading>();
}

unique_ptr<IDrawable> RasterRTShadowsDrawableFactory::create(const ConfigurationNode& config) const
{
	const auto& confObject = config.asObject();
	auto instance = confObject.keyExists("Sampler")
		? UniformSamplerFactory().create(confObject["Sampler"])
		: UniformSamplerFactory().create();
	return make_unique<RasterRTShadowsShading>(move(instance));
}
