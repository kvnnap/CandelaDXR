#pragma once

#include <string>

#include "feanor/core/environment/resource_manager.h"
#include "feanor/core/configuration/configuration_node.h"
#include "feanor/core/configuration/parser/iparser.h"

#include "Renderer/Camera.h"
#include "Renderer/IRenderer.h"
#include "Renderer/IDrawable.h"
#include "Scene/Scene.h"
#include "Scene/ISceneLoader.h"
#include "Animation/IAnimation.h"

namespace candela::environment
{
    using ConfigurationManager = feanor::environment::ResourceManager<feanor::configuration::parser::Parser>;
    using CameraManager = feanor::environment::ResourceManager<renderer::Camera>;
    using SceneManager = feanor::environment::ResourceManager<scene::Scene>;
    using SceneLoaderManager = feanor::environment::ResourceManager<scene::ISceneLoader>;
    using RendererManager = feanor::environment::ResourceManager<renderer::IRenderer>;
    using DrawableManager = feanor::environment::ResourceManager<renderer::IDrawable>;
    using AnimationManager = feanor::environment::ResourceManager<animation::IAnimation>;

    class Environment
    {
    public:
        Environment();
        void bootstrap(const std::string& configPath);

        ConfigurationManager& getConfigurationManager();
        CameraManager& getCameraManager();
        SceneLoaderManager& getSceneLoaderManager();
        SceneManager& getSceneManager();
        RendererManager& getRendererManager();
        DrawableManager& getDrawableManager();
        AnimationManager& getAnimationManager();

        static Environment& getInstance();
    private:
        void loadCoreFactories();

        // Loaded configuration
        feanor::configuration::ConfigurationNodePt configuration;

        // Managers
        ConfigurationManager configurationManager;
        CameraManager cameraManager;
        SceneManager sceneManager;
        SceneLoaderManager sceneLoaderManager;
        DrawableManager drawableManager;
        AnimationManager animationManager;
        RendererManager rendererManager;
    };
}