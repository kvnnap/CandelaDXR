#include "WebUtil.h"

#include "Window/WindowsDef.h"
#include <wininet.h>
#include <string>
#include <iostream>
#include <cstdint>
#include <memory>

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetLastErrorAsString()
{
	//Get the error message ID, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0) {
		return std::string(); //No error message has been recorded
	}

	LPSTR messageBuffer = nullptr;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	//Copy the error message into a std::string.
	std::string message(messageBuffer, size);

	//Free the Win32's string's buffer.
	LocalFree(messageBuffer);

	return message;
}

//download file from WWW in a buffer
bool candela::util::WWWFileBuffer(const std::string& host, const std::string& path, const std::string& headers, std::vector<unsigned char>& outData)
{
	struct InternetHandle
	{
		InternetHandle(HINTERNET handle)
			: handle(handle)
		{}

		~InternetHandle()
		{
			if (handle)
				InternetCloseHandle(handle);
		}

		operator bool() { return handle != NULL; }
		operator HINTERNET() const { return handle; }
		HINTERNET handle;
	};

	InternetHandle hInternet = InternetOpen(TEXT("CandelaDXR"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!hInternet)
		return false;

	InternetHandle hConnect = InternetConnect(hInternet, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
	if (!hConnect)
		return false;

	LPCTSTR acceptTypes[2] = { TEXT("*/*"), NULL };
	InternetHandle hRequest = HttpOpenRequest(hConnect, TEXT("GET"), path.c_str(), NULL, NULL, acceptTypes, INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
	if (!hRequest)
		return false;

	bool ret = HttpSendRequest(hRequest, headers.data(), static_cast<DWORD>(headers.size()), NULL, 0);
	if (!ret)
		return false;

	std::int32_t statusCode{};
	DWORD statusCodeLen = sizeof(statusCode);
	ret = HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeLen, 0);
	if (!ret)
		return false;

	if (statusCode != 200)
		return false;
	
	outData.clear();
	constexpr unsigned int outBuffSize = 8192;
	auto outBuffer = std::make_unique<unsigned char[]>(outBuffSize);
	while (true)
	{
		DWORD dwSize;
		if (!InternetReadFile(hRequest, (LPVOID)outBuffer.get(), outBuffSize - 1, &dwSize))
			return false;
		if (dwSize == 0)
			break;
		//outData += std::string(outBuffer.get(), dwSize);
		outData.insert(outData.end(), outBuffer.get(), outBuffer.get() + dwSize);
	}

	return true;
}
