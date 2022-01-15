#include "WindowException.h"
#include <sstream>
#include <DXGIMessages.h>

using std::string;
using std::ostringstream;
using std::hex;
using std::dec;
using std::endl;

using candela::exception::Exception;
using candela::exception::WindowException;

WindowException::WindowException(const std::string& fileName, int lineNumber, const std::string& functionName, HRESULT hr)
	: Exception(fileName, lineNumber, functionName)
{
	/*MAKELANGID

	GetLocaleInfo(LOCALE_USER_DEFAULT,)*/
	char* msgBuf = nullptr;
	DWORD msgLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&msgBuf), 0, nullptr);

	ostringstream ss;

	ss << whatBuffer << "HR Code:\t 0x" << hex << hr << dec << endl;

	if (msgLen == 0) {
		ss << "WindowEx:\t Unidentified Error Code" << endl;
	}
	else {
		string errorString = msgBuf;
		LocalFree(msgBuf);

		ss << "HR Str :\t" << errorString << endl;
	}

	whatBuffer = ss.str();
}
