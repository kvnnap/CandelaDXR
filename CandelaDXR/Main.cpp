// CandelaDXR.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "Window/Window.h"

#include "feanor/core/io/imousewriter.h"

using candela::ui::Window;

int main()
{
    struct TestWriter 
        : public feanor::io::IMouseWriter
    {
        void pressKey(uint8_t key) override
        {
            std::cout << (int)key << std::endl;
        }
        void depressKey(uint8_t key) override {}
        void updatePosition(uint16_t x, uint16_t y) override
        {
            //std::cout << x << ", " << y << std::endl;
        }
        
        void scroll(int units) override {}
    };
    
    TestWriter testWriter;
    Window wnd("test", 800, 600, nullptr, &testWriter);
    while (!wnd.ProcessMessages(true));
    std::cout << "Hello World!\n";
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
