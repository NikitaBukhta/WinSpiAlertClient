#include "StringUtils.h"

#include <windows.h>

namespace utils
{

std::wstring StringUtils::to_wide(std::string_view ascii)
{
	return {ascii.begin(), ascii.end()};
}

std::string StringUtils::to_utf8(const std::wstring_view wide)
{
	if (wide.empty())
		return {};

	const int wide_size = static_cast<int>(wide.size());
	const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_size, nullptr, 0, nullptr, nullptr);
	std::string utf8(static_cast<size_t>(size), '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_size, utf8.data(), size, nullptr, nullptr);
	return utf8;
}

} // namespace utils
