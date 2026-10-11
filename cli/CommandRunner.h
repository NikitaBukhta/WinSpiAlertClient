#ifndef WINSPIALERTCLIENT_COMMANDRUNNER_H
#define WINSPIALERTCLIENT_COMMANDRUNNER_H

#include <cstdio>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

#include "ClientConfig.h"
#include "CommandLine.h"
#include "app/SpialertService.h"

namespace cli
{

class CommandRunner
{
public:
	// NOTE: args are not copied, they must outlive the runner
	explicit CommandRunner(std::span<const wchar_t* const> args);

	int run();

private:
	int run_command(const CommandLine& command_line);
	[[nodiscard]] int execute(const CommandLine& command_line) const;
	void create_service(const ClientConfig& config);
	[[nodiscard]] int run_status() const;
	[[nodiscard]] int run_send(const std::filesystem::path& image_path) const;
	[[nodiscard]] int run_list(bool pending) const;
	static int fail(const std::string& message);
	static void print_help(FILE* stream);

private:
	std::span<const wchar_t* const> _args;
	std::unique_ptr<app::SpialertService> _service;
};

} // namespace cli

#endif // WINSPIALERTCLIENT_COMMANDRUNNER_H
