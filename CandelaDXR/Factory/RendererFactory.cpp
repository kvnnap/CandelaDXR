#include "RendererFactory.h"

#include "Environment/Environment.h"
#include "Renderer/Renderer.h"
#include "Renderer/IDrawable.h"
#include "VectorFactory.h"

#include <vector>
#include <cstdint>

using std::unique_ptr;
using std::make_unique;
using std::vector;

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
	auto& drawablesConfig = config["Drawables"].asList();
	bool debugEnabled = false;
	bool breakEnabled = false;
	bool vsync = false;
	std::uint32_t adapterIndex = 0;
	if (config.asObject().keyExists("AdapterIndex"))
		adapterIndex = config["AdapterIndex"].read<std::uint32_t>();
	if (config.asObject().keyExists("Debug"))
		debugEnabled = config["Debug"].read<bool>();
	if (config.asObject().keyExists("Break"))
		breakEnabled = config["Break"].read<bool>();
	if (config.asObject().keyExists("VSync"))
		vsync = config["VSync"].read<bool>();
	std::vector<IDrawable*> drawables;
	for (auto& drawableConfig : drawablesConfig)
		drawables.push_back(&env.getDrawableManager().getInstanceManager().get(drawableConfig));
	auto renderer = make_unique<Renderer>(scene, camera, dim, std::move(drawables), adapterIndex, debugEnabled, breakEnabled, vsync);
	return renderer;
}
