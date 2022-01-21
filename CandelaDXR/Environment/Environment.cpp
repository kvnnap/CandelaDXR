#include <memory>
#include <stdexcept>

#include "Environment.h"

#include "feanor/core/configuration/parser/json_configuration_parser.h"
#include "Scene/SceneFactory.h"
#include "Scene/WavefrontSceneLoaderFactory.h"

using candela::environment::Environment;
using candela::environment::ConfigurationManager;
using candela::environment::SceneLoaderManager;
using candela::environment::SceneManager;

using feanor::configuration::ObjectNode;
using feanor::configuration::LiteralNode;
using feanor::configuration::ConfigurationNode;

using feanor::configuration::parser::JsonConfigurationParserFactory;
using candela::scene::factory::SceneFactory;
using candela::scene::factory::WavefrontSceneLoaderFactory;

using std::string;
using std::make_unique;
using std::runtime_error;

Environment::Environment()
{
    loadCoreFactories();
}

ConfigurationManager& Environment::getConfigurationManager()
{
    return configurationManager;
}

SceneLoaderManager& Environment::getSceneLoaderManager()
{
    return sceneLoaderManager;
}

SceneManager& Environment::getSceneManager()
{
    return sceneManager;
}

void Environment::bootstrap(const string& configPath)
{
    // Load configuration
    auto& configFactoryManager = configurationManager.getFactoryManager();

    ObjectNode objectNode;
    objectNode.add("file", make_unique<ConfigurationNode>(LiteralNode(string(configPath))));

    for (auto& configFactory : configFactoryManager)
    {
        auto configInstance = configFactory->create(objectNode);
        auto configType = string(configInstance->getType());

        // compare file extension
        if (configType.size() > configPath.size())
            continue;

        if (!std::equal(configType.rbegin(), configType.rend(), configPath.rbegin()))
            continue;

        configuration = configInstance->loadConfiguration();
        break;
    }

    if (!configuration)
        throw runtime_error("Cannot load configuration");

    if (!configuration->isObject())
        throw runtime_error("Configuration root node should be an object");

    // Load the sections
    sceneManager.loadSection("Scenes", configuration);
    sceneLoaderManager.loadSection("SceneLoaders", configuration);
}

Environment& Environment::getInstance()
{
    static Environment instance;
    return instance;
}

void Environment::loadCoreFactories()
{
    // Register Configuration Parsers
    configurationManager.getFactoryManager().registerItem<JsonConfigurationParserFactory>("JsonConfigurationParser");

    // Register Scenes
    sceneManager.getFactoryManager().registerItem<SceneFactory>("Scene", *this);

    // Register Scene Loaders
    sceneLoaderManager.getFactoryManager().registerItem<WavefrontSceneLoaderFactory>("WavefrontSceneLoader", *this);
}
