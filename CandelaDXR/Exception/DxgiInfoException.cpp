#include <sstream>
#include "DxgiInfoException.h"

using std::string;
using std::ostringstream;
using std::endl;

using candela::directx::DxgiInfoManager;
using candela::exception::Exception;
using candela::exception::DxgiInfoException;

DxgiInfoException::DxgiInfoException(const std::string& fileName, int lineNumber, const std::string& functionName, const DxgiInfoManager& infoManager)
	: Exception(fileName, lineNumber, functionName)
{
	ostringstream ss;

	ss << whatBuffer;

	auto messages = infoManager.getMessages();
	for (const auto& message : messages)
		ss << message << endl;

	whatBuffer = ss.str();
}
