#include "SceneFactory.h"
#include "Environment/Environment.h"
#include "LightFactory.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::environment::Environment;
using candela::scene::Scene;
using candela::scene::factory::SceneFactory;
using candela::scene::factory::LightFactory;

SceneFactory::SceneFactory(Environment& env)
	: env(env)
{
}

unique_ptr<Scene> SceneFactory::create() const
{
	return make_unique<Scene>();
}

unique_ptr<Scene> SceneFactory::create(const ConfigurationNode& config) const
{
	auto scene = make_unique<Scene>();

	const auto &configObj = config.asObject();

	if (configObj.keyExists("Lights"))
	{
		std::uint32_t count{};
		const auto& configLights = configObj["Lights"].asList();
		for (const auto& configLight : configLights)
		{
			auto externalLight = LightFactory().create(configLight);
			auto &lightNode = scene->getSceneGraph().addChild("_config_light_" + std::to_string(count++));
			scene->addExternalLight({ *externalLight, &lightNode });
		}
	}

	return scene;
}
