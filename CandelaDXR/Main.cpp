// CandelaDXR.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>


#include "Exception/Exception.h"
#include "Environment/Environment.h"

using std::cout;
using std::endl;
using std::string;

using candela::exception::Exception;
using candela::environment::Environment;


int main(int argc, char** argv)
{
    //struct TestWriter 
    //    : public feanor::io::IMouseWriter
    //{
    //    void pressKey(uint8_t key) override
    //    {
    //        cout << (int)key << endl;
    //    }
    //    void depressKey(uint8_t key) override {}
    //    void updatePosition(uint16_t x, uint16_t y) override
    //    {
    //        //cout << x << ", " << y << endl;
    //    }
    //    
    //    void scroll(int units) override {}
    //};
    //
    //cout << "Material Size: " << sizeof(Material) << endl;
    //TestWriter testWriter;
    //Window wnd("test", 800, 600, nullptr, &testWriter);
    //while (!wnd.ProcessMessages(true));
    //std::cout << "Hello World!\n";

    string err;

    try {
        // Get config file name
        string configFileName = "config.json";
        if (argc == 2)
            configFileName = argv[1];

        // Start environment
        Environment env;
        env.bootstrap(configFileName);

        // Invoke scene loaders - this will populate shapes and primitives
        for (auto sceneLoader : env.getSceneLoaderManager().getInstanceManager().asList())
            sceneLoader->loadScene();

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

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
