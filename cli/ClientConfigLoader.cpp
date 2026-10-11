#include "ClientConfigLoader.h"

#include <algorithm>
#include <format>
#include <system_error>

#include "CommandLineOptions.h"
#include "rapidjson/error/en.h"
#include "rapidjson/reader.h"
#include "utils/FileUtils.h"
#include "utils/SizeUnits.h"

#define DEFAULT_CONFIG_PATH "secret/config.json"
#define CONFIG_EXAMPLE_PATH "secret/config.json.example"
#define MAX_CONFIG_FILE_SIZE (64 * KIB)
#define REQUIRED_SCHEME "https"
#define FIRST_KEY_CHAR '!'
#define LAST_KEY_CHAR '~'

namespace cli
{

using utils::FileUtils;

ClientConfigLoader::ClientConfigLoader(ConfigSource source) : _source(std::move(source))
{
}

std::expected<ClientConfig, std::string> ClientConfigLoader::load()
{
	// NOTE: lets the client reach the mock server on a machine without secret/config.json
	if (_source.url && _source.key && !_source.config_path)
	{
		apply_source();
		return build();
	}

	auto json = read_config_file();
	if (!json)
		return std::unexpected(json.error());

	return parse(std::move(*json));
}

std::expected<ClientConfig, std::string> ClientConfigLoader::parse(std::string json)
{
	const auto parsed = parse_json(std::move(json));
	if (!parsed)
		return std::unexpected(parsed.error());

	apply_source();
	return build();
}

std::expected<std::string, std::string> ClientConfigLoader::read_config_file() const
{
	const std::filesystem::path path = config_path();
	const std::string path_text = FileUtils::to_utf8(path);

	std::error_code error;
	const bool exists = std::filesystem::is_regular_file(path, error);
	if (!exists && _source.config_path)
		return std::unexpected(std::format("config file {} not found", path_text));
	if (!exists)
		return std::unexpected(std::format("config file {} not found: copy {} to {} or pass {} and {}", path_text,
		                                   CONFIG_EXAMPLE_PATH, DEFAULT_CONFIG_PATH, OPTION_URL, OPTION_KEY));

	auto content = FileUtils::read_file(path, MAX_CONFIG_FILE_SIZE);
	if (!content)
		return std::unexpected(
			std::format("cannot read config file {} or it is larger than {} bytes", path_text, MAX_CONFIG_FILE_SIZE));

	return std::move(*content);
}

std::expected<void, std::string> ClientConfigLoader::parse_json(std::string json)
{
	rapidjson::Reader reader;
	rapidjson::InsituStringStream stream(json.data());
	ConfigFileHandler handler(_base_url, _api_key);

	const rapidjson::ParseResult result = reader.Parse<rapidjson::kParseInsituFlag>(stream, handler);
	if (result.IsError())
		return std::unexpected(std::format("config file {} is not valid JSON: {} (offset {})",
		                                   FileUtils::to_utf8(config_path()),
		                                   rapidjson::GetParseError_En(result.Code()), result.Offset()));
	if (!handler.wrong_type_key.empty())
		return std::unexpected(std::format("{} in config file {} must be a string", handler.wrong_type_key,
		                                   FileUtils::to_utf8(config_path())));
	return {};
}

void ClientConfigLoader::apply_source()
{
	if (_source.url)
		_base_url = _source.url;
	if (_source.key)
		_api_key = _source.key;
}

std::expected<ClientConfig, std::string> ClientConfigLoader::build() const
{
	const auto fields_checked = check_required_fields();
	if (!fields_checked)
		return std::unexpected(fields_checked.error());

	const auto key_checked = check_api_key();
	if (!key_checked)
		return std::unexpected(key_checked.error());

	const auto url = parse_base_url();
	if (!url)
		return std::unexpected(url.error());

	return ClientConfig{
		.host = url->host, .port = url->port, .base_path = normalize_path(url->path), .api_key = *_api_key};
}

std::expected<void, std::string> ClientConfigLoader::check_required_fields() const
{
	if (!_base_url || _base_url->empty())
		return std::unexpected("base_url is not set" + source_hint(OPTION_URL));
	if (!_api_key || _api_key->empty())
		return std::unexpected("api_key is not set" + source_hint(OPTION_KEY));
	return {};
}

// NOTE: the key goes into the X-API-Key header as is: a CR or LF would split the header, non-ASCII would be garbled
std::expected<void, std::string> ClientConfigLoader::check_api_key() const
{
	const bool is_printable_ascii =
		std::ranges::all_of(*_api_key, [](const char ch) { return ch >= FIRST_KEY_CHAR && ch <= LAST_KEY_CHAR; });
	if (!is_printable_ascii)
		return std::unexpected("api_key must contain only printable ASCII characters without spaces");
	return {};
}

std::expected<utils::Url, std::string> ClientConfigLoader::parse_base_url() const
{
	auto url = utils::UrlParser::parse(*_base_url);
	if (!url)
		return std::unexpected(std::format("base_url is not a valid URL: {}", *_base_url));
	if (url->has_user_info)
		return std::unexpected("base_url must not contain a user name or password");
	if (url->scheme != REQUIRED_SCHEME)
		return std::unexpected(std::format("base_url must use https: {}", *_base_url));
	if (!url->query_and_fragment.empty())
		return std::unexpected(std::format("base_url must not contain a query or fragment: {}", *_base_url));
	return std::move(*url);
}

std::filesystem::path ClientConfigLoader::config_path() const
{
	return _source.config_path.value_or(DEFAULT_CONFIG_PATH);
}

std::string ClientConfigLoader::source_hint(std::string_view flag) const
{
	return std::format(": add it to {} or pass {}", FileUtils::to_utf8(config_path()), flag);
}

std::string ClientConfigLoader::normalize_path(std::string path)
{
	while (!path.empty() && path.back() == '/')
		path.pop_back();
	return path;
}

} // namespace cli
