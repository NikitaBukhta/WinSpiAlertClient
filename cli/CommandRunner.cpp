#include "CommandRunner.h"

#include <cstdlib>
#include <print>

#include "ClientConfigLoader.h"
#include "CommandLineParser.h"
#include "ConsoleRetryWaiter.h"
#include "api/SpialertApi.h"
#include "model/WicImageCompressorFactory.h"
#include "net/WinHttpClient.h"
#include "utils/FileUtils.h"

#define USER_AGENT "WinSpiAlertClient/1.0"
#define HELP_HINT "See 'client.exe help'."

namespace cli
{

CommandRunner::CommandRunner(std::span<const wchar_t* const> args) : _args(args)
{
}

int CommandRunner::run()
{
	if (_args.empty())
	{
		print_help(stderr);
		return EXIT_FAILURE;
	}

	const auto command_line = CommandLineParser(_args).parse();
	if (!command_line)
		return fail(command_line.error());

	return run_command(*command_line);
}

int CommandRunner::run_command(const CommandLine& command_line)
{
	if (command_line.command == CommandLine::CommandEnum::HELP_COMMAND)
	{
		print_help(stdout);
		return EXIT_SUCCESS;
	}

	const auto config = ClientConfigLoader(command_line.source).load();
	if (!config)
		return fail(config.error());

	create_service(*config);
	return execute(command_line);
}

int CommandRunner::execute(const CommandLine& command_line) const
{
	switch (command_line.command)
	{
	case CommandLine::CommandEnum::STATUS_COMMAND:
		return run_status();
	case CommandLine::CommandEnum::SEND_COMMAND:
		return run_send(command_line.image_path);
	case CommandLine::CommandEnum::LIST_COMMAND:
		return run_list(command_line.pending);
	// NOTE: help is handled in run_command, before the config is loaded. No default: a new command must be added here
	case CommandLine::CommandEnum::HELP_COMMAND:
		break;
	}
	return EXIT_FAILURE;
}

void CommandRunner::create_service(const ClientConfig& config)
{
	auto http_client = std::make_unique<net::WinHttpClient>(config.host, USER_AGENT, config.port);
	_service = std::make_unique<app::SpialertService>(
		std::make_unique<api::SpialertApi>(std::move(http_client), config.base_path, config.api_key),
		std::make_unique<model::WicImageCompressorFactory>(), std::make_unique<ConsoleRetryWaiter>());
}

int CommandRunner::run_status() const
{
	const auto state = _service->is_online();
	if (!state)
	{
		std::println(stderr, "{}", state.error());
		return EXIT_FAILURE;
	}

	switch (*state)
	{
	case app::SpialertService::ServerStateEnum::ONLINE_STATE:
		std::println("Server is online");
		return EXIT_SUCCESS;
	case app::SpialertService::ServerStateEnum::OFFLINE_STATE:
		break;
	}
	std::println("Server is not online");
	return EXIT_FAILURE;
}

int CommandRunner::run_send(const std::filesystem::path& image_path) const
{
	const auto state = _service->is_online();
	if (!state)
	{
		std::println(stderr, "{}", state.error());
		return EXIT_FAILURE;
	}
	// TODO: wait for an offline server with the backoff of send_with_retry instead of failing at once
	if (*state != app::SpialertService::ServerStateEnum::ONLINE_STATE)
	{
		std::println(stderr, "Server is not online");
		return EXIT_FAILURE;
	}

	const std::string image_name = utils::FileUtils::to_utf8(image_path);
	const auto sent = _service->send_img(image_path);
	if (!sent)
	{
		std::println(stderr, "Failed to send {}: {}", image_name, sent.error());
		return EXIT_FAILURE;
	}

	std::println("Sent {}", image_name);
	return EXIT_SUCCESS;
}

int CommandRunner::run_list(bool pending) const
{
	// TODO: request events from the server
	std::println(stderr, "list is not implemented yet");
	return EXIT_FAILURE;
}

int CommandRunner::fail(const std::string& message)
{
	std::println(stderr, "error: {}", message);
	std::println(stderr, HELP_HINT);
	return EXIT_FAILURE;
}

void CommandRunner::print_help(FILE* stream)
{
	std::print(stream, "{}", R"(Usage: client.exe [options] <command> [args]

Commands:
  status              check that the server is online
  send <image>        create an event and upload a PNG or JPEG image to it
  list [--pending]    list events; --pending: only events without an image
  help                show this help

Options:
  --config <path>     config file (default: secret/config.json)
  --url <url>         server base URL, used instead of base_url from the config file
  --key <key>         API key, used instead of api_key from the config file.
                      The key stays in the shell history and is visible in the
                      process list, so prefer the config file.
  --                  end of options, e.g. send -- -capture.png

Exit code: 0 on success, 1 on error.
)");
}

} // namespace cli
