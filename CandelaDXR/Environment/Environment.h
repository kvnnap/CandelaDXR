#pragma once

#include <string>
#include <vector>

#include "feanor/core/environment/resource_manager.h"
#include "feanor/core/configuration/configuration_node.h"
#include "feanor/core/configuration/parser/iparser.h"

#include "Renderer/Camera.h"
#include "Renderer/IRenderer.h"
#include "Renderer/IDrawable.h"
#include "Scene/Scene.h"
#include "Scene/ISceneLoader.h"
#include "Scene/ISceneModifier.h"
#include "Animation/IAnimation.h"
#include "Chain/IChain.h"

namespace candela::environment
{
    using ConfigurationManager = feanor::environment::ResourceManager<feanor::configuration::parser::Parser>;
    using CameraManager = feanor::environment::ResourceManager<renderer::Camera>;
    using SceneManager = feanor::environment::ResourceManager<scene::Scene>;
    using SceneLoaderManager = feanor::environment::ResourceManager<scene::ISceneLoader>;
    using SceneModifierManager = feanor::environment::ResourceManager<scene::ISceneModifier>;
    using RendererManager = feanor::environment::ResourceManager<renderer::IRenderer>;
    using DrawableManager = feanor::environment::ResourceManager<renderer::IDrawable>;
    using AnimationManager = feanor::environment::ResourceManager<animation::IAnimation>;
    using ChainManager = feanor::environment::ResourceManager<chain::IChain>;
    using ChainListManager = feanor::environment::ResourceManager<chain::CFList>;

    class Environment
    {
    public:
        Environment();
        void setArguments(std::vector<std::string>&& p_argv);
        void setArguments(int argc, char **argv);
        void bootstrap(const std::string& configPath);

        const std::vector<std::string>& getArguments() const;
        ConfigurationManager& getConfigurationManager();
        CameraManager& getCameraManager();
        SceneLoaderManager& getSceneLoaderManager();
        SceneManager& getSceneManager();
        RendererManager& getRendererManager();
        DrawableManager& getDrawableManager();
        AnimationManager& getAnimationManager();
        ChainManager& getChainManager();
        ChainListManager& getChainListManager();

        static Environment& getInstance();
    private:
        void loadCoreFactories();

        // Main arguments
        std::vector<std::string> argv;

        // Loaded configuration
        feanor::configuration::ConfigurationNodePt configuration;

        // Managers
        ConfigurationManager configurationManager;
        CameraManager cameraManager;
        SceneManager sceneManager;
        SceneLoaderManager sceneLoaderManager;
        SceneModifierManager sceneModifierManager;
        DrawableManager drawableManager;
        AnimationManager animationManager;
        RendererManager rendererManager;
        ChainManager chainManager;
        ChainListManager chainListManager;
    };
}