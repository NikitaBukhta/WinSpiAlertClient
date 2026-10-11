#ifndef WINSPIALERTCLIENT_CLIENTCONFIG_H
#define WINSPIALERTCLIENT_CLIENTCONFIG_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace cli
{

struct ClientConfig
{
	std::string host;
	uint16_t port{};
	std::string base_path;
	std::string api_key;
};

struct ConfigSource
{
	std::optional<std::filesystem::path> config_path;
	std::optional<std::string> url;
	std::optional<std::string> key;
};

} // namespace cli

#endif // WINSPIALERTCLIENT_CLIENTCONFIG_H
