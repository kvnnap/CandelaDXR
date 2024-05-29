#include <memory>
#include <stdexcept>

#include "Environment.h"

#include "feanor/core/configuration/parser/json_configuration_parser.h"
#include "Factory/SceneFactory.h"
#include "Factory/WavefrontSceneLoaderFactory.h"
#include "Factory/AssImpSceneLoaderFactory.h"
#include "Factory/SceneModifierFactory.h"
#include "Factory/RendererFactory.h"
#include "Factory/RasterDrawableFactory.h"
#include "Factory/RasterRTShadowsDrawableFactory.h"
#include "Factory/LightTracingDrawableFactory.h"
#include "Factory/PathTracingDrawableFactory.h"
#include "Factory/DenoiserDrawableFactory.h"
#include "Factory/OptixDenoiserDrawableFactory.h"
#include "Factory/CameraFactory.h"
#include "Factory/AnimationFactory.h"
#include "Factory/ChainFactory.h"

using candela::environment::Environment;
using candela::environment::ConfigurationManager;
using candela::environment::CameraManager;
using candela::environment::SceneLoaderManager;
using candela::environment::SceneManager;
using candela::environment::RendererManager;
using candela::environment::DrawableManager;
using candela::environment::AnimationManager;
using candela::environment::ChainManager;

using feanor::configuration::ObjectNode;
using feanor::configuration::LiteralNode;
using feanor::configuration::ConfigurationNode;

using feanor::configuration::parser::JsonConfigurationParserFactory;
using candela::scene::factory::SceneFactory;
using candela::scene::factory::WavefrontSceneLoaderFactory;
using candela::scene::factory::AssImpSceneLoaderFactory;
using candela::scene::factory::SceneModifierFactory;
using candela::renderer::factory::RendererFactory;
using candela::renderer::factory::RasterDrawableFactory;
using candela::renderer::factory::RasterRTShadowsDrawableFactory;
using candela::renderer::factory::LightTracingDrawableFactory;
using candela::renderer::factory::PathTracingDrawableFactory;
using candela::renderer::factory::DenoiserDrawableFactory;
using candela::renderer::factory::OptixDenoiserDrawableFactory;
using candela::renderer::factory::CameraFactory;
using candela::animation::factory::AnimationFactory;

using candela::chain::factory::ChainFactory;

using std::string;
using std::vector;
using std::make_unique;
using std::runtime_error;

Environment::Environment()
{
    loadCoreFactories();
}

void Environment::setArguments(vector<string>&& p_argv)
{
    argv = std::move(p_argv);
}

void Environment::setArguments(int argc, char** argv)
{
    this->argv = vector<string>(argv, argv + argc);
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

    if (configuration->asObject().keyExists("SceneModifiers"))
        sceneModifierManager.loadSection("SceneModifiers", configuration);

    // Invoke scene loaders - this will populate shapes and primitives
    for (auto sceneLoader : sceneLoaderManager.getInstanceManager().asList())
        sceneLoader->loadScene();

    // Invoke scene modifiers - this will modify the scene materials/lights/etc
    for (auto sceneModifier : sceneModifierManager.getInstanceManager().asList())
        sceneModifier->modifyScene();

    // Load animations
    if (configuration->asObject().keyExists("Animations"))
        animationManager.loadSection("Animations", configuration);

    // Load Drawables - Passes
    drawableManager.loadSection("Drawables", configuration);

    // Load chains
    if (configuration->asObject().keyExists("Chains"))
        chainManager.loadSection("Chains", configuration);
    
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
    sceneLoaderManager.getFactoryManager().registerItem<AssImpSceneLoaderFactory>("AssImpSceneLoader", *this);

    // Register Scene Modifiers
    sceneModifierManager.getFactoryManager().registerItem<SceneModifierFactory>("SceneModifier", *this);

    // Renderers
    rendererManager.getFactoryManager().registerItem<RendererFactory>("Renderer", *this);

    // Drawables
    drawableManager.getFactoryManager().registerItem<RasterDrawableFactory>("RasterDrawable");
    drawableManager.getFactoryManager().registerItem<RasterRTShadowsDrawableFactory>("RasterRTShadowsDrawable");
    drawableManager.getFactoryManager().registerItem<LightTracingDrawableFactory>("LightTracingDrawable");
    drawableManager.getFactoryManager().registerItem<PathTracingDrawableFactory>("PathTracingDrawable");
    drawableManager.getFactoryManager().registerItem<DenoiserDrawableFactory>("DenoiserDrawable");
    drawableManager.getFactoryManager().registerItem<OptixDenoiserDrawableFactory>("OptixDenoiserDrawable");

    // Cameras
    cameraManager.getFactoryManager().registerItem<CameraFactory>("Camera");

    // Animation
    animationManager.getFactoryManager().registerItem<AnimationFactory>("Animation");

    // Chains
    chainManager.getFactoryManager().registerItem<ChainFactory>("Chain");
}

const vector<string>& Environment::getArguments() const
{
    return argv;
}

ConfigurationManager& Environment::getConfigurationManager() { return configurationManager; }
CameraManager& Environment::getCameraManager() { return cameraManager; }
SceneLoaderManager& Environment::getSceneLoaderManager() { return sceneLoaderManager; }
SceneManager& Environment::getSceneManager() { return sceneManager; }
RendererManager& Environment::getRendererManager() { return rendererManager; }
DrawableManager& Environment::getDrawableManager() { return drawableManager; }
AnimationManager& Environment::getAnimationManager() { return animationManager; }
ChainManager& Environment::getChainManager() { return chainManager; }

