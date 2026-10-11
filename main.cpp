#include <span>

#include "cli/CommandRunner.h"

int wmain(int argc, wchar_t** argv)
{
	const std::span<const wchar_t* const> args(argv, static_cast<size_t>(argc));
	// NOTE: argv[0] is the program name. subspan(1) of an empty span is undefined behavior
	const auto user_args = args.empty() ? args : args.subspan(1);
	return cli::CommandRunner(user_args).run();
}
