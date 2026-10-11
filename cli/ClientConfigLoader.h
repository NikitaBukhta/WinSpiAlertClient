#ifndef WINSPIALERTCLIENT_CLIENTCONFIGLOADER_H
#define WINSPIALERTCLIENT_CLIENTCONFIGLOADER_H

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "ClientConfig.h"
#include "utils/SaxHandler.h"
#include "utils/UrlParser.h"

namespace cli
{

struct ConfigFileHandler : utils::BaseSaxHandler<ConfigFileHandler>
{
	std::optional<std::string>& base_url;
	std::optional<std::string>& api_key;
	std::string wrong_type_key;

	// NOTE: arrays count too, otherwise strings inside "base_url": [...] would land in base_url
	int depth{};

	ConfigFileHandler(std::optional<std::string>& target_base_url, std::optional<std::string>& target_api_key)
		: base_url(target_base_url), api_key(target_api_key)
	{
	}

	bool String(const char* str, rapidjson::SizeType length, bool copy) const
	{
		if (depth != 1)
			return true;

		if (current_key == "base_url")
			base_url.emplace(str, length);
		else if (current_key == "api_key")
			api_key.emplace(str, length);
		return true;
	}

	bool StartObject()
	{
		non_string_value();
		++depth;
		return true;
	}

	bool EndObject(rapidjson::SizeType member_count)
	{
		--depth;
		return true;
	}

	bool StartArray()
	{
		non_string_value();
		++depth;
		return true;
	}

	bool EndArray(rapidjson::SizeType element_count)
	{
		--depth;
		return true;
	}

	bool Null()
	{
		return non_string_value();
	}

	bool Bool(bool value)
	{
		return non_string_value();
	}

	bool Int(int value)
	{
		return non_string_value();
	}

	bool Uint(unsigned value)
	{
		return non_string_value();
	}

	bool Int64(int64_t value)
	{
		return non_string_value();
	}

	bool Uint64(uint64_t value)
	{
		return non_string_value();
	}

	bool Double(double value)
	{
		return non_string_value();
	}

	bool non_string_value()
	{
		const bool is_config_field = current_key == "base_url" || current_key == "api_key";
		if (depth == 1 && is_config_field)
			wrong_type_key = current_key;
		return true;
	}
};

class ClientConfigLoader
{
public:
	explicit ClientConfigLoader(ConfigSource source);

	// NOTE: one-shot, the state builds up in fields: call load() or parse() once per loader
	std::expected<ClientConfig, std::string> load();
	std::expected<ClientConfig, std::string> parse(std::string json);

private:
	[[nodiscard]] std::expected<std::string, std::string> read_config_file() const;
	std::expected<void, std::string> parse_json(std::string json);
	void apply_source();
	[[nodiscard]] std::expected<ClientConfig, std::string> build() const;
	[[nodiscard]] std::expected<void, std::string> check_required_fields() const;
	[[nodiscard]] std::expected<void, std::string> check_api_key() const;
	[[nodiscard]] std::expected<utils::Url, std::string> parse_base_url() const;
	[[nodiscard]] std::filesystem::path config_path() const;
	[[nodiscard]] std::string source_hint(std::string_view flag) const;
	static std::string normalize_path(std::string path);

private:
	ConfigSource _source;
	std::optional<std::string> _base_url;
	std::optional<std::string> _api_key;
};

} // namespace cli

#endif // WINSPIALERTCLIENT_CLIENTCONFIGLOADER_H
