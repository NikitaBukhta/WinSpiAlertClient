#include <cstdio>
#include <cstdlib>
#include <exception>
#include <print>
#include <span>

#include "cli/CommandRunner.h"

int wmain(int argc, wchar_t** argv)
{
	// NOTE: errors are reported by return values; this only keeps an unexpected exception (bad_alloc on a huge image,
	// for example) from ending the process without a word
	try
	{
		const std::span<const wchar_t* const> args(argv, static_cast<size_t>(argc));
		// NOTE: argv[0] is the program name. subspan(1) of an empty span is undefined behavior
		const auto user_args = args.empty() ? args : args.subspan(1);
		return cli::CommandRunner(user_args).run();
	}
	catch (const std::exception& error)
	{
		std::println(stderr, "Unexpected error: {}", error.what());
	}
	catch (...)
	{
		std::println(stderr, "Unexpected error");
	}
	return EXIT_FAILURE;
}
