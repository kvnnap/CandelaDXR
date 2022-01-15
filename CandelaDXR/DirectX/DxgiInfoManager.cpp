#include "Window/WindowsDef.h"
#include <memory>

#include "DxgiInfoManager.h"
#include "Exception/WindowException.h"

using namespace candela::directx;

// Static stuff
const char* DxgiInfoManager::hModuleName = "dxgidebug.dll";

using namespace std;

DxgiInfoManager::DxgiInfoManager()
	: next(0u)
{
	using DxgiGetDebugInterface = decltype(DXGIGetDebugInterface);

	HMODULE moduleHandle = LoadLibrary(hModuleName);
	if (moduleHandle == NULL)
		ThrowException("Cannot load module");

	try {
		DxgiGetDebugInterface* fn = reinterpret_cast<DxgiGetDebugInterface*>(GetProcAddress(moduleHandle, "DXGIGetDebugInterface"));
		if (fn == nullptr)
			ThrowException("Cannot find function DXGIGetDebugInterface");

		HRESULT hr;
		WinThrowIfFailed(fn(__uuidof(IDXGIInfoQueue), &pDxgiInfoQueue));
	} 
	catch (...) {
		FreeLibrary(moduleHandle);
		throw;
	}
}

DxgiInfoManager::~DxgiInfoManager() {
	pDxgiInfoQueue.Reset();
	HMODULE moduleHandle = GetModuleHandle(hModuleName);
	if (moduleHandle != NULL)
		FreeLibrary(moduleHandle);
}

void DxgiInfoManager::set() noexcept
{
	next = pDxgiInfoQueue->GetNumStoredMessages(DXGI_DEBUG_ALL);
}

bool DxgiInfoManager::hasMessages() const
{
	const auto end = pDxgiInfoQueue->GetNumStoredMessages(DXGI_DEBUG_ALL);
	return end > next;
}

std::vector<std::string> DxgiInfoManager::getMessages() const
{
	vector<string> messages;
	const auto end = pDxgiInfoQueue->GetNumStoredMessages(DXGI_DEBUG_ALL);

	for (auto i = next; i < end; ++i)
	{
		SIZE_T messageLength = 0;

		pDxgiInfoQueue->GetMessage(DXGI_DEBUG_ALL, i, nullptr, &messageLength);

		auto bytes = make_unique<char[]>(messageLength);
		auto pMessage = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(bytes.get());

		pDxgiInfoQueue->GetMessage(DXGI_DEBUG_ALL, i, pMessage, &messageLength);
		messages.emplace_back(pMessage->pDescription);
	}

	return messages;
}
