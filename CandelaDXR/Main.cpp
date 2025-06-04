#include <iostream>
#include <string>

#include "Exception/Exception.h"
#include "Environment/Environment.h"
#include "Window/Window.h"
#include "Renderer/IRenderer.h"
#include "Version/Version.h"

#include "feanor/anvil/core/anvil.h"

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
        if (argc >= 2)
            configFileName = argv[1];

        // Start environment
        Environment env;
        env.setArguments(argc, argv);
        env.bootstrap(configFileName);

        IRenderer& renderer = env.getRendererManager().getInstanceManager().get(0);
        renderer.init();

        // Assume always parallel - hence Mutex
        std::optional<int> exitCode;
        while (true)
        {
            ANVIL_IMGUI({
                exitCode = Window::ProcessMessages();
            });

            if (exitCode)
                break;

            renderer.renderFrame();
            
            // Below moved at end of renderer
            //feanor::anvil::Anvil::getInstance().tick();
        }

        return *exitCode;
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

    // Cleanup Anvil - since it's currently static, do it here
    ANVIL_CODE_RAW(feanor::anvil::Anvil::getInstance().destroy();)

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
