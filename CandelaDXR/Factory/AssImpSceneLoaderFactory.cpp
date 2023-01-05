#include "AssImpSceneLoaderFactory.h"

#include "Scene/AssImpSceneLoader.h"

#include "Environment/Environment.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::environment::Environment;
using candela::scene::ISceneLoader;
using candela::scene::AssImpSceneLoader;
using candela::scene::factory::AssImpSceneLoaderFactory;

AssImpSceneLoaderFactory::AssImpSceneLoaderFactory(environment::Environment& env)
	: env(env)
{
}

unique_ptr<ISceneLoader> AssImpSceneLoaderFactory::create() const
{
	return unique_ptr<ISceneLoader>();
}

unique_ptr<ISceneLoader> AssImpSceneLoaderFactory::create(const ConfigurationNode& config) const
{
	auto sceneLoader = make_unique<AssImpSceneLoader>(&env.getSceneManager().getInstanceManager().get(config["Scene"]));
	sceneLoader->setFilePath(config["FilePath"].read<std::string>());
	if (config.asObject().keyExists("AlwaysComputeNormals"))
		sceneLoader->setAlwaysComputeNormals(config["AlwaysComputeNormals"].read<bool>());
	return sceneLoader;
}
