#include "SceneModifierFactory.h"
#include "Environment/Environment.h"

#include "Scene/SceneModifier.h"
#include "VectorFactory.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::environment::Environment;
using candela::scene::ISceneModifier;
using candela::scene::SceneModifier;
using candela::scene::factory::SceneModifierFactory;
using candela::mathematics::Vector3;
using candela::mathematics::factory::Vector3Factory;

SceneModifierFactory::SceneModifierFactory(environment::Environment& env)
	: env(env)
{
}

unique_ptr<ISceneModifier> SceneModifierFactory::create() const
{
	return unique_ptr<ISceneModifier>();
}

unique_ptr<ISceneModifier> SceneModifierFactory::create(const ConfigurationNode& config) const
{
	auto sceneModifier = make_unique<SceneModifier>(&env.getSceneManager().getInstanceManager().get(config["Scene"]));

	if (config.asObject().keyExists("Materials"))
	{
		for (const auto& matConf : config["Materials"].asList())
		{
			for (const auto& item : matConf.asObject())
			{
				std::string name;
				std::size_t id{};
				if (matConf.asObject().keyExists("Name"))
					name = matConf["Name"].read<std::string>();
				else
					id = matConf["Id"].read<std::size_t>();

				if (item.key == "Diffuse" || item.key == "Emissive" || item.key == "Specular" || item.key == "TransmissiveFilter")
				{
					sceneModifier->addProperty(Property<Vector3>{
						.Type = "Material",
							.Name = name,
							.PropertyName = item.key,
							.Id = id,
							.Data = *Vector3Factory().create(item.value)
					});
				}
				else if (item.key == "Dissolve" || item.key == "RefractiveIndex")
				{
					sceneModifier->addProperty(Property<float>{
						.Type = "Material",
						.Name = name,
						.PropertyName = item.key,
						.Id = id,
						.Data = item.value.read<float>()
					});
				}
			} 
		}

	}
	
	return sceneModifier;
}
