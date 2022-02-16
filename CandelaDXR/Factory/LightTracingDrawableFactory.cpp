#include "LightTracingDrawableFactory.h"

#include "Renderer/LightTracingShading.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::renderer::IDrawable;
using candela::renderer::LightTracingShading;
using candela::renderer::factory::LightTracingDrawableFactory;

unique_ptr<IDrawable> LightTracingDrawableFactory::create() const
{
	return make_unique<LightTracingShading>();
}

unique_ptr<IDrawable> LightTracingDrawableFactory::create(const ConfigurationNode& config) const
{
	return make_unique<LightTracingShading>();
}
