#include "RendererFactory.h"

#include "Environment/Environment.h"
#include "Renderer/Renderer.h"
#include "VectorFactory.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::environment::Environment;
using candela::renderer::IRenderer;
using candela::renderer::Renderer;
using candela::renderer::factory::RendererFactory;
using candela::mathematics::factory::UVector2Factory;

RendererFactory::RendererFactory(Environment& env)
	: env(env)
{
}

unique_ptr<IRenderer> RendererFactory::create() const
{
	return unique_ptr<Renderer>();
}

unique_ptr<IRenderer> RendererFactory::create(const ConfigurationNode& config) const
{
	auto scene = &env.getSceneManager().getInstanceManager().get(config["Scene"]);
	auto camera = &env.getCameraManager().getInstanceManager().get(config["Camera"]);
	auto dim = *UVector2Factory().create(config["WindowDimensions"]);
	auto renderer = make_unique<Renderer>(scene, camera, dim);
	return renderer;
}
