#include "StringUtils.h"

namespace utils
{

std::wstring StringUtils::to_wide(std::string_view ascii)
{
	return {ascii.begin(), ascii.end()};
}

} // namespace utils
