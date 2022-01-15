#include "WindowClass.h"
#include "Exception/Exception.h"

using candela::ui::WindowClass;

WindowClass::WindowClass(const std::string& className, WNDPROC wndProc)
	: className(className)
{
	HINSTANCE hInstance = GetModuleHandle(NULL);

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW; // redraw when resized
	wc.lpfnWndProc = wndProc;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = className.c_str();
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	// Register the class
	if (RegisterClassEx(&wc))
		this->className = className;
	else
		ThrowException("Error registering class");
}

const std::string& WindowClass::getClassName() const
{
	return className;
}

WindowClass::~WindowClass()
{
	UnregisterClass(className.c_str(), GetModuleHandle(NULL));
}

