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
	const auto& confObject = config.asObject();
	bool gBuffer{};
	if (confObject.keyExists("ComputeGBuffer"))
		gBuffer = confObject["ComputeGBuffer"].read<bool>();
	auto rs = make_unique<RasterShading>(gBuffer);
	if (confObject.keyExists("ComputeRadiance"))
		rs->setComputeRadiance(confObject["ComputeRadiance"].read<bool>());
	if (confObject.keyExists("ComputeEmissiveIfRadOff"))
		rs->setComputeEmissiveIfRadOff(confObject["ComputeEmissiveIfRadOff"].read<bool>());
	return rs;
}
