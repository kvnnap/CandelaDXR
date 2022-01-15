#include <sstream>

#include "Exception.h"

using std::string;
using std::endl;
using std::ostringstream;

using candela::exception::Exception;

Exception::Exception(const string& fileName, int lineNumber, const string& functionName, const string& reason)
{
	ostringstream ss;

	if (!reason.empty())
		ss << "Reason:\t" << reason << endl;

	ss << "FileName:\t" << fileName << endl
		<< "Line:\t" << lineNumber << endl
		<< "Fn:\t" << functionName << endl;

	whatBuffer = ss.str();
}

const char* Exception::what() const
{
	return whatBuffer.c_str();
}
