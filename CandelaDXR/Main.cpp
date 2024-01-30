#include <iostream>
#include <string>

#include "Exception/Exception.h"
#include "Environment/Environment.h"
#include "Window/Window.h"
#include "Renderer/IRenderer.h"
#include "Version/Version.h"

using std::cout;
using std::endl;
using std::string;

using candela::exception::Exception;
using candela::environment::Environment;
using candela::renderer::IRenderer;
using candela::ui::Window;

int main(int argc, char** argv)
{
    cout << "Version: " << candela::version::CommitSummary() << " Date: " << candela::version::Date << endl;

    string err;

    try {
        // Get config file name
        string configFileName = "Assets/config.json";
        if (argc == 2)
            configFileName = argv[1];

        // Start environment
        Environment env;
        env.setArguments(argc, argv);
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

//#include "Util/WebUtil.h"

//{
//    Environment env;
//    std::string headers = "Accept: application/vnd.github.v3+json\nAuthorization: token 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";
//    //std::string response;
//    std::vector<uint8_t> response;
//    candela::util::WWWFileBuffer("api.github.com", "/repos/kvnnap/CandelaDXR/actions/runs?per_page=1&branch=master", headers, response);
//    auto &parser = env.getConfigurationManager().getFactoryManager().get("JsonConfigurationParser");
//    
//    //WWWFileBuffer("api.github.com", "/repos/kvnnap/CandelaDXR/actions/runs/2115995498/artifacts", response);
//    cout << response.size() << endl;
//}
// Need string: obj.workflow_runs[0].artifacts_url and then obj.artifacts[0].archive_download_url
// Regex: [^.\[\]]+|(?:\[(\d+)\])
// Cross-platform: Use libcurl and unzip libraries
