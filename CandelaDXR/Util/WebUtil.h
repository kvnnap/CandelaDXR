#pragma once

#include <vector>
#include <string>

namespace candela::util
{
	bool WWWFileBuffer(const std::string& host, const std::string& path, const std::string& headers, std::vector<unsigned char>& outData);
}