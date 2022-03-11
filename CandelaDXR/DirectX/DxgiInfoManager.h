#pragma once

#include <dxgidebug.h>
#include <wrl/client.h>
#include <vector>
#include <string>

#include "System/DllManager.h"

namespace candela::directx
{
	class DxgiInfoManager
	{
	public:
		DxgiInfoManager();
		virtual ~DxgiInfoManager();

		DxgiInfoManager(const DxgiInfoManager&) = delete;
		DxgiInfoManager& operator=(const DxgiInfoManager&) = delete;

		void set() noexcept;

		bool hasMessages() const;
		std::vector<std::string> getMessages() const;

	private:
		system::DllManager dxgiDebugDll;

		UINT64 next;
		Microsoft::WRL::ComPtr<IDXGIInfoQueue> pDxgiInfoQueue;
	};
}
