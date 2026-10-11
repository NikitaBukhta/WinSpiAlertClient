#ifndef WINSPIALERTCLIENT_TESTS_CLIENTPROCESS_H
#define WINSPIALERTCLIENT_TESTS_CLIENTPROCESS_H

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "HandlePtr.h"

namespace test_helpers
{

struct ClientOutput
{
	bool started{};
	unsigned long exit_code{};
	std::string out;
	std::string err;
};

// NOTE: runs client.exe in an empty temp directory, so the default secret/config.json is never found
class ClientProcess
{
public:
	ClientProcess();
	~ClientProcess();

	ClientProcess(const ClientProcess& other) = delete;
	ClientProcess& operator=(const ClientProcess& other) = delete;

	// NOTE: args are quoted as is, so they must not contain quotes or end with a backslash
	[[nodiscard]] ClientOutput run(const std::vector<std::wstring>& args) const;

private:
	[[nodiscard]] std::expected<unsigned long, std::string> execute(const std::vector<std::wstring>& args) const;

	[[nodiscard]] HandlePtr create_output_file(const std::filesystem::path& path) const;
	[[nodiscard]] unsigned long wait_for_exit(const HandlePtr& process) const;
	static std::wstring build_command_line(const std::vector<std::wstring>& args);
	static std::string read_output(const std::filesystem::path& path);

private:
	std::filesystem::path _directory;
	std::filesystem::path _out_path;
	std::filesystem::path _err_path;
};

} // namespace test_helpers

#endif // WINSPIALERTCLIENT_TESTS_CLIENTPROCESS_H
