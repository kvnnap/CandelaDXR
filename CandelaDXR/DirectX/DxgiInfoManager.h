#pragma once

#include <wrl/client.h>
#include <dxgidebug.h>
#include <vector>
#include <string>

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
		static const char* hModuleName;

		UINT64 next;
		Microsoft::WRL::ComPtr<IDXGIInfoQueue> pDxgiInfoQueue;
	};
}
