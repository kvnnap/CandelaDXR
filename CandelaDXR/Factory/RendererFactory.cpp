#include "RendererFactory.h"

#include "Environment/Environment.h"
#include "Renderer/Renderer.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::environment::Environment;
using candela::renderer::IRenderer;
using candela::renderer::Renderer;
using candela::renderer::factory::RendererFactory;

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
	auto renderer = make_unique<Renderer>(&env.getSceneManager().getInstanceManager().get(config["Scene"]));
	return renderer;
}
