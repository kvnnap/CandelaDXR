#include "RasterDrawableFactory.h"

#include "Renderer/RasterShading.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::renderer::IDrawable;
using candela::renderer::RasterShading;
using candela::renderer::factory::RasterDrawableFactory;

unique_ptr<IDrawable> RasterDrawableFactory::create() const
{
	return make_unique<RasterShading>();
}

unique_ptr<IDrawable> RasterDrawableFactory::create(const ConfigurationNode& config) const
{
	return make_unique<RasterShading>();
}
