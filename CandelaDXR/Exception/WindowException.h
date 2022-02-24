#pragma once

#include "Exception.h"

#include "Window/WindowsDef.h"

namespace candela::exception 
{
	class WindowException :
		public Exception
	{
	public:
		WindowException(const std::string& fileName, int lineNumber, const std::string& functionName, HRESULT hr);
		virtual ~WindowException() = default;
	};
}

#define ThrowWindowException(hr) (throw candela::exception::WindowException(__FILE__, __LINE__, __func__, hr))
#define WinThrowIfFailed(expr) if(FAILED(hr = (expr))) ThrowWindowException(hr)
#define GFXTHROWIFFAILED(expr) WinThrowIfFailed(expr)