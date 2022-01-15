#pragma once

#include "Window/WindowsDef.h"

#include <string>

namespace candela::ui
{
	class WindowClass
	{
	public:
		WindowClass(const std::string& className, WNDPROC wndProc);
		~WindowClass();

		const std::string& getClassName() const;

	private:
		std::string className;
	};
}
