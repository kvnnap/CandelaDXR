#pragma once

#include <exception>
#include <string>

namespace candela::exception
{
	class Exception
		: public std::exception
	{
	public:
		Exception(const std::string& fileName, int lineNumber, const std::string& fnName, const std::string& reason = "");
		virtual ~Exception() = default;

		const char* what() const override;

	protected:
		std::string whatBuffer;
	};
}

#define ThrowException(reason) (throw candela::exception::Exception(__FILE__, __LINE__, __func__, reason))
