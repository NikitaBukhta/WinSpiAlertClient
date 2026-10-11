#include "ClientProcess.h"

#include <format>
#include <string>

#include "utils/FileUtils.h"
#include "utils/StringUtils.h"
#include "utils/SizeUnits.h"
#include "utils/UuidGenerator.h"

#define EXIT_TIMEOUT_MS 60'000
#define MAX_OUTPUT_SIZE MIB
#define TIMED_OUT_EXIT_CODE 0xDEAD

namespace test_helpers
{

ClientProcess::ClientProcess()
	: _directory(std::filesystem::temp_directory_path() / ("spialert-client-" + utils::UuidGenerator::generate())),
	  _out_path(_directory / "stdout.txt"), _err_path(_directory / "stderr.txt")
{
	std::filesystem::create_directories(_directory);
}

ClientProcess::~ClientProcess()
{
	std::error_code ignored;
	std::filesystem::remove_all(_directory, ignored);
}

ClientOutput ClientProcess::run(const std::vector<std::wstring>& args) const
{
	const auto exit_code = execute(args);
	if (!exit_code)
		return {.started = false, .err = exit_code.error()};

	return {.started = true, .exit_code = *exit_code, .out = read_output(_out_path), .err = read_output(_err_path)};
}

std::expected<unsigned long, std::string> ClientProcess::execute(const std::vector<std::wstring>& args) const
{
	const HandlePtr out_file = create_output_file(_out_path);
	const HandlePtr err_file = create_output_file(_err_path);

	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdOutput = out_file.get();
	startup.hStdError = err_file.get();

	std::wstring command_line = build_command_line(args);
	PROCESS_INFORMATION process{};
	const BOOL created = CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, TRUE, 0, nullptr,
	                                    _directory.c_str(), &startup, &process);
	if (!created)
		return std::unexpected(std::format("cannot start {}: error {}", CLIENT_EXE, GetLastError()));

	const HandlePtr process_handle{process.hProcess};
	const HandlePtr thread_handle{process.hThread};
	return wait_for_exit(process_handle);
}

HandlePtr ClientProcess::create_output_file(const std::filesystem::path& path) const
{
	SECURITY_ATTRIBUTES inheritable{};
	inheritable.nLength = sizeof(inheritable);
	inheritable.bInheritHandle = TRUE;

	const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inheritable, CREATE_ALWAYS,
	                                FILE_ATTRIBUTE_NORMAL, nullptr);
	return HandlePtr{file == INVALID_HANDLE_VALUE ? nullptr : file};
}

unsigned long ClientProcess::wait_for_exit(const HandlePtr& process) const
{
	const bool exited = WaitForSingleObject(process.get(), EXIT_TIMEOUT_MS) == WAIT_OBJECT_0;
	if (!exited)
	{
		TerminateProcess(process.get(), TIMED_OUT_EXIT_CODE);
		return TIMED_OUT_EXIT_CODE;
	}

	DWORD exit_code = 0;
	GetExitCodeProcess(process.get(), &exit_code);
	return exit_code;
}

std::wstring ClientProcess::build_command_line(const std::vector<std::wstring>& args)
{
	std::wstring command_line = L"\"" + utils::StringUtils::to_wide(CLIENT_EXE) + L"\"";
	for (const std::wstring& arg : args)
		command_line += L" \"" + arg + L"\"";
	return command_line;
}

std::string ClientProcess::read_output(const std::filesystem::path& path)
{
	std::string output = utils::FileUtils::read_file(path, MAX_OUTPUT_SIZE).value_or("");
	// NOTE: stdout and stderr are in text mode, so the client writes "\r\n" for every "\n"
	std::erase(output, '\r');
	return output;
}

} // namespace test_helpers
