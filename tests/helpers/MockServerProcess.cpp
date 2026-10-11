// winsock2.h must come before windows.h (included by MockServerProcess.h), otherwise winsock.h definitions clash
#include <winsock2.h>
#include <ws2tcpip.h>

#include "MockServerProcess.h"

#include <chrono>
#include <thread>

#include "utils/StringUtils.h"

using utils::StringUtils;

// MOCK_SERVER_PYTHON, MOCK_SERVER_SCRIPT and MOCK_SERVER_LOG come from CMake
#define STARTUP_TIMEOUT_MS 15'000
#define STARTUP_POLL_INTERVAL_MS 100

namespace test_helpers
{

MockServerProcess::MockServerProcess(const std::string& host, unsigned short port, const std::string& extra_args)
	: _host(host), _port(port)
{
	_listening = start(extra_args) && wait_until_listening();
}

MockServerProcess::~MockServerProcess()
{
	if (_job)
		TerminateJobObject(_job.get(), 0);
	if (_process)
		WaitForSingleObject(_process.get(), INFINITE);
}

bool MockServerProcess::is_listening() const noexcept
{
	return _listening;
}

unsigned short MockServerProcess::find_free_port(const std::string& host)
{
	WSADATA wsa_data{};
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
		return 0;

	const SOCKET probe = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = 0; // the OS picks a free port
	inet_pton(AF_INET, host.c_str(), &address.sin_addr);

	unsigned short port = 0;
	int address_len = sizeof(address);
	const bool bound = bind(probe, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 &&
	                   getsockname(probe, reinterpret_cast<sockaddr*>(&address), &address_len) == 0;
	if (bound)
		port = ntohs(address.sin_port);

	closesocket(probe);
	WSACleanup();
	return port;
}

bool MockServerProcess::start(const std::string& extra_args)
{
	_job.reset(CreateJobObjectW(nullptr, nullptr));
	if (!_job)
		return false;

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	const BOOL limits_set =
		SetInformationJobObject(_job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits));
	if (!limits_set)
		return false;

	// output goes to a file: a pipe nobody reads would fill up and block the mock's log
	const HandlePtr log_file = open_log_file();
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	startup.hStdOutput = log_file.get();
	startup.hStdError = log_file.get();

	std::wstring command_line = StringUtils::to_wide("\"" MOCK_SERVER_PYTHON "\" \"" MOCK_SERVER_SCRIPT "\" --host " +
	                                                 _host + " --port " + std::to_string(_port) + " " + extra_args);
	PROCESS_INFORMATION process{};
	const BOOL created = CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr,
	                                    nullptr, &startup, &process);
	if (!created)
		return false;

	_process.reset(process.hProcess);
	const HandlePtr thread{process.hThread};

	// assigned before it runs: the mock cannot start children outside the job
	const BOOL assigned = AssignProcessToJobObject(_job.get(), process.hProcess);
	ResumeThread(process.hThread);
	return assigned;
}

bool MockServerProcess::wait_until_listening() const
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{STARTUP_TIMEOUT_MS};
	while (std::chrono::steady_clock::now() < deadline)
	{
		const bool exited = WaitForSingleObject(_process.get(), 0) == WAIT_OBJECT_0;
		if (exited)
			return false;
		if (can_connect(_host, _port))
			return true;
		std::this_thread::sleep_for(std::chrono::milliseconds{STARTUP_POLL_INTERVAL_MS});
	}
	return false;
}

bool MockServerProcess::can_connect(const std::string& host, unsigned short port)
{
	WSADATA wsa_data{};
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
		return false;

	const SOCKET probe = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	inet_pton(AF_INET, host.c_str(), &address.sin_addr);

	const bool connected = connect(probe, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;

	closesocket(probe);
	WSACleanup();
	return connected;
}

HandlePtr MockServerProcess::open_log_file()
{
	SECURITY_ATTRIBUTES inheritable{};
	inheritable.nLength = sizeof(inheritable);
	inheritable.bInheritHandle = TRUE;

	const HANDLE file =
		CreateFileW(StringUtils::to_wide(MOCK_SERVER_LOG).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
		            &inheritable, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	return HandlePtr{file == INVALID_HANDLE_VALUE ? nullptr : file};
}

} // namespace test_helpers
