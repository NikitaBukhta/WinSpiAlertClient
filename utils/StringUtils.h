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
};
} // namespace utils

#endif // WINSPIALERTCLIENT_STRINGUTILS_H
