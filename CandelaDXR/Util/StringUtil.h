#pragma once

#include <string>

namespace candela::util
{
	std::string WStringToString(const std::wstring &str);
	std::wstring StringToWString(const std::string &str);
	std::string ToLower(const std::string& str);
}
