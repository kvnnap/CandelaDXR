#pragma once

#include "Exception.h"
#include "DirectX/DxgiInfoManager.h"

namespace candela::exception
{
	class DxgiInfoException :
		public Exception
	{
	public:
		DxgiInfoException(const std::string& fileName, int lineNumber, const std::string& functionName, const candela::directx::DxgiInfoManager& infoManager);
		virtual ~DxgiInfoException() = default;
	};
}

#define ThrowDxgiInfoExceptionIfFailed(expr) { infoManager.set(); expr; if (infoManager.hasMessages()) { throw candela::exception::DxgiInfoException(__FILE__, __LINE__, __func__, infoManager); } }
