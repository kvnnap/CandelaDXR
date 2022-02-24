// CandelaDXR.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>


#include "Exception/Exception.h"
#include "Environment/Environment.h"
#include "Window/Window.h"
#include "Renderer/IRenderer.h"

using std::cout;
using std::endl;
using std::string;

using candela::exception::Exception;
using candela::environment::Environment;
using candela::renderer::IRenderer;
using candela::ui::Window;


int main(int argc, char** argv)
{
    string err;

    try {
        // Get config file name
        string configFileName = "Assets/config.json";
        if (argc == 2)
            configFileName = argv[1];

        // Start environment
        Environment env;
        env.bootstrap(configFileName);

        IRenderer& renderer = env.getRendererManager().getInstanceManager().get(0);
        renderer.init();

        while (!Window::ProcessMessages())
        {
            renderer.renderFrame();
        }

        return EXIT_SUCCESS;
    }
    catch (const Exception& e) {
        err = "App Exception: \n";
        err += e.what();
    }
    catch (const std::exception& e) {
        err = "Standard Exception: \n";
        err += e.what();
    }
    catch (...) {
        err = "Unknown Exception\n";
    }

    cout << err << endl;
    return EXIT_FAILURE;
}
