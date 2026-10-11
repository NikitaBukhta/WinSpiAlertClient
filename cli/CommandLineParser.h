#ifndef WINSPIALERTCLIENT_COMMANDLINEPARSER_H
#define WINSPIALERTCLIENT_COMMANDLINEPARSER_H

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "CommandLine.h"

namespace cli
{

class CommandLineParser
{
public:
	explicit CommandLineParser(std::span<const wchar_t* const> args);
	std::expected<CommandLine, std::string> parse();

private:
	struct CommandInfo
	{
		std::wstring_view name;
		CommandLine::CommandEnum command;
		std::string_view arguments; // for the error message, e.g. "<event_id> <image>"
		size_t argument_count;
	};

	[[nodiscard]] bool has_help_option() const;
	std::expected<void, std::string> read_arguments();
	std::expected<void, std::string> parse_option(std::wstring_view arg);
	bool parse_flag(std::string_view option);
	std::expected<void, std::string> parse_value_option(std::string_view option);
	std::expected<std::wstring_view, std::string> take_value(std::string_view option);
	std::expected<void, std::string> parse_command();
	[[nodiscard]] std::expected<void, std::string> check_argument_count(const CommandInfo& info) const;
	[[nodiscard]] std::expected<void, std::string> check_pending() const;
	static const CommandInfo* find_command(std::wstring_view name);
	static std::expected<int, std::string> parse_event_id(std::wstring_view arg);
	static bool is_option(std::wstring_view arg);
	static bool is_known_option(std::wstring_view arg);
	static bool is_end_of_options(std::wstring_view arg);

	template <typename T>
	static std::expected<void, std::string> assign_once(std::optional<T>& field, T value, std::string_view option);

private:
	std::span<const wchar_t* const> _args;
	size_t _position{};
	std::vector<std::wstring_view> _positionals;
	CommandLine _command_line;
};

} // namespace cli

#endif // WINSPIALERTCLIENT_COMMANDLINEPARSER_H
