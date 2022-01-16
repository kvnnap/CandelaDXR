#pragma once

#include <string>

#include "feanor/core/environment/resource_manager.h"
#include "feanor/core/configuration/configuration_node.h"

#include "feanor/core/configuration/parser/iparser.h"

namespace candela::environment
{
    using ConfigurationManager = feanor::environment::ResourceManager<feanor::configuration::parser::Parser>;

    class Environment
    {
    public:
        ConfigurationManager& getConfigurationManager();


        void bootstrap(const std::string& configPath);

        static Environment& getInstance();
    private:
        void loadCoreFactories();

        // Loaded configuration
        feanor::configuration::ConfigurationNodePt configuration;

        // Managers
        ConfigurationManager configurationManager;
    };
}