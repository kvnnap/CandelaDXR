#pragma once

#include <string>
#define WIN32_LEAN_AND_MEAN      // Exclude rarely-used stuff from Windows headers

#include <windows.h>

namespace candela::system
{
	class DllManager
	{
	public:
		DllManager(const std::string& dllName);
		virtual ~DllManager();

		DllManager(const DllManager&) = delete;
		DllManager& operator=(const DllManager&) = delete;

		

		template<class T>
		T* getFunction(const std::string& fnName)
		{
			return reinterpret_cast<T*>(getFn(fnName));
		}
	private:
		HMODULE hModule;

		void* getFn(const std::string& fnName);
	};
}
