#ifndef WINSPIALERTCLIENT_URLPARSER_H
#define WINSPIALERTCLIENT_URLPARSER_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace utils
{

struct Url
{
	std::string scheme;
	std::string host;
	uint16_t port{};
	std::string path;
	std::string query_and_fragment;
	bool has_user_info{};
};

class UrlParser
{
public:
	static std::optional<Url> parse(std::string_view url);

private:
	static bool is_ascii(std::string_view text);
};

} // namespace utils

#endif // WINSPIALERTCLIENT_URLPARSER_H
