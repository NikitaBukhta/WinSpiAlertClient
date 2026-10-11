#include "UrlParser.h"

#include <algorithm>
#include <cctype>

#include <windows.h>
#include <winhttp.h>

#include "StringUtils.h"

#define WHOLE_COMPONENT static_cast<DWORD>(-1)
#define ASCII_LIMIT 0x80

namespace utils
{

std::optional<Url> UrlParser::parse(const std::string_view url)
{
	if (url.empty() || !is_ascii(url))
		return std::nullopt;

	const std::wstring wide_url = StringUtils::to_wide(url);

	// NOTE: component lengths of -1 make WinHttpCrackUrl return pointers into wide_url instead of copies
	URL_COMPONENTS components{};
	components.dwStructSize = sizeof(components);
	components.dwSchemeLength = WHOLE_COMPONENT;
	components.dwHostNameLength = WHOLE_COMPONENT;
	components.dwUserNameLength = WHOLE_COMPONENT;
	components.dwPasswordLength = WHOLE_COMPONENT;
	components.dwUrlPathLength = WHOLE_COMPONENT;
	components.dwExtraInfoLength = WHOLE_COMPONENT;

	const auto url_length = static_cast<DWORD>(wide_url.size());
	const BOOL cracked = WinHttpCrackUrl(wide_url.c_str(), url_length, 0, &components);
	const bool has_host_and_port = components.dwHostNameLength > 0 && components.nPort != 0;
	if (!cracked || !has_host_and_port)
		return std::nullopt;

	Url result;
	result.scheme = StringUtils::to_utf8({components.lpszScheme, components.dwSchemeLength});
	std::ranges::transform(result.scheme, result.scheme.begin(), [](unsigned char ch) { return std::tolower(ch); });
	result.host = StringUtils::to_utf8({components.lpszHostName, components.dwHostNameLength});
	result.port = components.nPort;
	result.path = StringUtils::to_utf8({components.lpszUrlPath, components.dwUrlPathLength});
	result.query_and_fragment = StringUtils::to_utf8({components.lpszExtraInfo, components.dwExtraInfoLength});
	result.has_user_info = components.dwUserNameLength > 0 || components.dwPasswordLength > 0;
	return result;
}

bool UrlParser::is_ascii(std::string_view text)
{
	return std::ranges::all_of(text, [](const unsigned char ch) { return ch < ASCII_LIMIT; });
}

} // namespace utils
