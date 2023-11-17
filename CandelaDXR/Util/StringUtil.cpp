#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "StringUtil.h"

#include <algorithm>
#include <codecvt>

using std::string;
using std::wstring;
using std::codecvt_utf8;
using std::wstring_convert;

string candela::util::WStringToString(const wstring& str)
{
	return wstring_convert<codecvt_utf8<wchar_t>, wchar_t>().to_bytes(str);
}

wstring candela::util::StringToWString(const string& str)
{
	return wstring_convert<codecvt_utf8<wchar_t>, wchar_t>().from_bytes(str).c_str();
}

string candela::util::ToLower(const string& str)
{
	string ret (str.size(), '\0');
	std::transform(str.begin(), str.end(), ret.begin(), [](unsigned char c) { return std::tolower(c); });
	return ret;
}
