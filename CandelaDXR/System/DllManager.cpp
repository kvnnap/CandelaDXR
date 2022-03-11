#include "DllManager.h"

#include "Exception/Exception.h"



using std::string;

using candela::system::DllManager;

DllManager::DllManager(const std::string& dllName)
	: hModule(NULL)
{
	hModule = LoadLibrary(dllName.c_str());
	if (hModule == NULL)
		ThrowException("Cannot load module: " + string(dllName));
}

DllManager::~DllManager()
{
	if (hModule != NULL)
		FreeLibrary(hModule);
}

void* DllManager::getFn(const std::string& fnName)
{
	auto fn = GetProcAddress(hModule, fnName.c_str());
	if (fn == nullptr)
		ThrowException("Cannot find function: '" + fnName + "'");
	return fn;
}
