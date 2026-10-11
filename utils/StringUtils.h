#ifndef WINSPIALERTCLIENT_STRINGUTILS_H
#define WINSPIALERTCLIENT_STRINGUTILS_H

#include <string>
#include <string_view>

namespace utils
{

class StringUtils
{
public:
	static std::wstring to_wide(std::string_view ascii);
	static std::string to_utf8(std::wstring_view wide);
};
} // namespace utils

#endif // WINSPIALERTCLIENT_STRINGUTILS_H
