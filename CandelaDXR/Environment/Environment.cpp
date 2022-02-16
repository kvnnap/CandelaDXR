#include <memory>
#include <stdexcept>

#include "Environment.h"

#include "feanor/core/configuration/parser/json_configuration_parser.h"
#include "factory/SceneFactory.h"
#include "factory/WavefrontSceneLoaderFactory.h"
#include "factory/RendererFactory.h"
#include "factory/RasterDrawableFactory.h"
#include "factory/LightTracingDrawableFactory.h"
#include "factory/CameraFactory.h"

using candela::environment::Environment;
using candela::environment::ConfigurationManager;
using candela::environment::CameraManager;
using candela::environment::SceneLoaderManager;
using candela::environment::SceneManager;
using candela::environment::RendererManager;
using candela::environment::DrawableManager;

using feanor::configuration::ObjectNode;
using feanor::configuration::LiteralNode;
using feanor::configuration::ConfigurationNode;

using feanor::configuration::parser::JsonConfigurationParserFactory;
using candela::scene::factory::SceneFactory;
using candela::scene::factory::WavefrontSceneLoaderFactory;
using candela::renderer::factory::RendererFactory;
using candela::renderer::factory::RasterDrawableFactory;
using candela::renderer::factory::LightTracingDrawableFactory;
using candela::renderer::factory::CameraFactory;

using std::string;
using std::make_unique;
using std::runtime_error;

Environment::Environment()
{
    loadCoreFactories();
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
    cameraManager.loadSection("Cameras", configuration);
    sceneManager.loadSection("Scenes", configuration);
    sceneLoaderManager.loadSection("SceneLoaders", configuration);

    // Invoke scene loaders - this will populate shapes and primitives
    for (auto sceneLoader : sceneLoaderManager.getInstanceManager().asList())
        sceneLoader->loadScene();

    // Load Drawables - Passes
    drawableManager.loadSection("Drawables", configuration);

    // Load renderers
    rendererManager.loadSection("Renderers", configuration);
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

    // Renderers
    rendererManager.getFactoryManager().registerItem<RendererFactory>("Renderer", *this);

    // Drawables
    drawableManager.getFactoryManager().registerItem<RasterDrawableFactory>("RasterDrawable");
    drawableManager.getFactoryManager().registerItem<LightTracingDrawableFactory>("LightTracingDrawable");

    // Cameras
    cameraManager.getFactoryManager().registerItem<CameraFactory>("Camera");
}

ConfigurationManager& Environment::getConfigurationManager() { return configurationManager; }
CameraManager& Environment::getCameraManager() { return cameraManager; }
SceneLoaderManager& Environment::getSceneLoaderManager() { return sceneLoaderManager; }
SceneManager& Environment::getSceneManager() { return sceneManager; }
RendererManager& Environment::getRendererManager() { return rendererManager; }
DrawableManager& Environment::getDrawableManager() { return drawableManager; }
