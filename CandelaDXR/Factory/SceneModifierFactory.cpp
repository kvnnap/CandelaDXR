#include "SceneModifierFactory.h"
#include "Environment/Environment.h"

#include "Scene/SceneModifier.h"
#include "VectorFactory.h"
#include "MathematicsFactory.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::environment::Environment;
using candela::scene::ISceneModifier;
using candela::scene::SceneModifier;
using candela::scene::factory::SceneModifierFactory;
using candela::mathematics::Vector3;
using candela::mathematics::Matrix;
using candela::mathematics::TransformComponents;
using candela::mathematics::factory::Vector3Factory;
using candela::mathematics::factory::TransformComponentsFactory;

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

	const auto& configObject = config.asObject();

	if (configObject.keyExists("Materials"))
	{
		for (const auto& matConf : configObject["Materials"].asList())
		{
			for (const auto& item : matConf.asObject())
			{
				std::vector<std::pair<std::string, std::size_t>> state;

				if (matConf.asObject().keyExists("Names"))
				{
					for (const auto& x : matConf["Names"].asList())
						state.emplace_back(x.read<std::string>(), 0);
				}
				if (matConf.asObject().keyExists("Ids"))
				{
					for (const auto& x : matConf["Ids"].asList())
						state.emplace_back("", x.read<std::size_t>());
				}

				for (const auto& x : state)
				{
					std::string name = x.first;
					std::size_t id = x.second;

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
	}
	else if (configObject.keyExists("Transforms"))
	{
		for (const auto& transConf : configObject["Transforms"].asList())
		{
			auto transformComponents = *TransformComponentsFactory().create(transConf);
			auto useCentrePos = true;
			auto translationAbsolute = false;
			auto relativeComponents = false;

			const auto& transConfObj = transConf.asObject();
			if (transConfObj.keyExists("UseCentrePosition"))
				useCentrePos = transConfObj["UseCentrePosition"].read<bool>();
			if (transConfObj.keyExists("TranslationAbsolute"))
				translationAbsolute = transConfObj["TranslationAbsolute"].read<bool>();
			if (transConfObj.keyExists("RelativeComponents"))
				relativeComponents = transConfObj["RelativeComponents"].read<bool>();

			for (const auto& name : transConf["Names"].asList())
			{
				sceneModifier->addProperty(Property<TransformComponents>{
					.Type = "SceneNode",
					.Name = name.read<std::string>(),
					.PropertyName = "Transform",
					.Id = 0,
					.ExtraConfig = {
						{"UseCentrePosition", useCentrePos ? "True" : "False"},
						{"TranslationAbsolute", translationAbsolute ? "True" : "False"},
						{"RelativeComponents", relativeComponents ? "True" : "False"},
					},
					.Data = transformComponents,
				});
			}
		}
	}
	
	return sceneModifier;
}
