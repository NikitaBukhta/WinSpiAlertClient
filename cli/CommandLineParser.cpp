#include "CommandLineParser.h"

#include <array>
#include <filesystem>
#include <format>

#include "CommandLineOptions.h"
#include "utils/StringUtils.h"

namespace cli
{

using utils::StringUtils;

CommandLineParser::CommandLineParser(std::span<const wchar_t* const> args) : _args(args)
{
}

std::expected<CommandLine, std::string> CommandLineParser::parse()
{
	// NOTE: help wins over the rest of the line, even over a wrong option before it
	if (has_help_option())
	{
		_command_line.command = CommandLine::CommandEnum::HELP_COMMAND;
		return _command_line;
	}

	const auto arguments_read = read_arguments();
	if (!arguments_read)
		return std::unexpected(arguments_read.error());

	const auto command_parsed = parse_command();
	if (!command_parsed)
		return std::unexpected(command_parsed.error());

	const auto pending_checked = check_pending();
	if (!pending_checked)
		return std::unexpected(pending_checked.error());

	return _command_line;
}

bool CommandLineParser::has_help_option() const
{
	for (const wchar_t* arg : _args)
	{
		const std::string option = StringUtils::to_utf8(arg);
		if (option == END_OF_OPTIONS)
			return false;
		if (option == OPTION_HELP || option == OPTION_HELP_SHORT)
			return true;
	}
	return false;
}

std::expected<void, std::string> CommandLineParser::read_arguments()
{
	while (_position < _args.size())
	{
		const std::wstring_view arg = _args[_position++];
		if (is_end_of_options(arg))
		{
			_positionals.insert(_positionals.end(), _args.begin() + _position, _args.end());
			break;
		}

		if (!is_option(arg))
		{
			_positionals.push_back(arg);
			continue;
		}

		const auto parsed = parse_option(arg);
		if (!parsed)
			return parsed;
	}
	return {};
}

std::expected<void, std::string> CommandLineParser::parse_option(std::wstring_view arg)
{
	const std::string option = StringUtils::to_utf8(arg);
	if (parse_flag(option))
		return {};
	return parse_value_option(option);
}

bool CommandLineParser::parse_flag(std::string_view option)
{
	if (option != OPTION_PENDING)
		return false;

	_command_line.pending = true;
	return true;
}

std::expected<void, std::string> CommandLineParser::parse_value_option(std::string_view option)
{
	const bool takes_value = option == OPTION_CONFIG || option == OPTION_URL || option == OPTION_KEY;
	if (!takes_value)
		return std::unexpected(std::format("unknown option {}", option));

	auto value = take_value(option);
	if (!value)
		return std::unexpected(value.error());

	ConfigSource& source = _command_line.source;
	if (option == OPTION_CONFIG)
		return assign_once(source.config_path, std::filesystem::path(*value), option);
	if (option == OPTION_URL)
		return assign_once(source.url, StringUtils::to_utf8(*value), option);
	return assign_once(source.key, StringUtils::to_utf8(*value), option);
}

template <typename T>
std::expected<void, std::string> CommandLineParser::assign_once(std::optional<T>& field, T value,
                                                                std::string_view option)
{
	if (field)
		return std::unexpected(std::format("{} is given more than once", option));
	field = std::move(value);
	return {};
}

std::expected<std::wstring_view, std::string> CommandLineParser::take_value(std::string_view option)
{
	const std::wstring_view next = _position < _args.size() ? _args[_position] : L"";
	const bool is_value = !next.empty() && !is_option(next);
	if (!is_value)
		return std::unexpected(std::format("{} requires a value", option));

	++_position;
	return next;
}

std::expected<void, std::string> CommandLineParser::parse_command()
{
	if (_positionals.empty())
		return std::unexpected("no command given");

	const CommandInfo* info = find_command(_positionals.front());
	if (!info)
		return std::unexpected(std::format("unknown command '{}'", StringUtils::to_utf8(_positionals.front())));

	const auto count_checked = check_argument_count(*info);
	if (!count_checked)
		return count_checked;

	_command_line.command = info->command;
	if (!info->argument.empty())
		_command_line.image_path = _positionals[1];
	return {};
}

std::expected<void, std::string> CommandLineParser::check_argument_count(const CommandInfo& info) const
{
	const size_t expected_count = info.argument.empty() ? 0 : 1;
	const size_t given_count = _positionals.size() - 1;
	if (given_count < expected_count)
		return std::unexpected(std::format("{} requires {}", StringUtils::to_utf8(info.name), info.argument));
	if (given_count > expected_count)
		return std::unexpected(
			std::format("unexpected argument '{}'", StringUtils::to_utf8(_positionals[expected_count + 1])));
	return {};
}

std::expected<void, std::string> CommandLineParser::check_pending() const
{
	const bool pending_without_list =
		_command_line.pending && _command_line.command != CommandLine::CommandEnum::LIST_COMMAND;
	if (pending_without_list)
		return std::unexpected(std::format("{} is valid only for the list command", OPTION_PENDING));
	return {};
}

const CommandLineParser::CommandInfo* CommandLineParser::find_command(std::wstring_view name)
{
	static constexpr auto COMMANDS = std::to_array<CommandInfo>({
		{.name = L"status", .command = CommandLine::CommandEnum::STATUS_COMMAND, .argument = ""},
		{.name = L"send", .command = CommandLine::CommandEnum::SEND_COMMAND, .argument = "<image>"},
		{.name = L"list", .command = CommandLine::CommandEnum::LIST_COMMAND, .argument = ""},
		{.name = L"help", .command = CommandLine::CommandEnum::HELP_COMMAND, .argument = ""},
	});

	for (const CommandInfo& info : COMMANDS)
	{
		if (info.name == name)
			return &info;
	}
	return nullptr;
}

bool CommandLineParser::is_option(std::wstring_view arg)
{
	return arg.size() > 1 && arg.front() == L'-';
}

bool CommandLineParser::is_end_of_options(std::wstring_view arg)
{
	return StringUtils::to_utf8(arg) == END_OF_OPTIONS;
}

} // namespace cli
