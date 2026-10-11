#ifndef WINSPIALERTCLIENT_COMMANDLINE_H
#define WINSPIALERTCLIENT_COMMANDLINE_H

#include <cstdint>
#include <filesystem>

#include "ClientConfig.h"

namespace cli
{

struct CommandLine
{
	enum class CommandEnum : uint8_t
	{
		STATUS_COMMAND,
		SEND_COMMAND,
		LIST_COMMAND,
		HELP_COMMAND,
	};

	CommandEnum command{CommandEnum::HELP_COMMAND};
	std::filesystem::path image_path;
	bool pending{};
	ConfigSource source;
};

} // namespace cli

#endif // WINSPIALERTCLIENT_COMMANDLINE_H
