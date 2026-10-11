#ifndef WINSPIALERTCLIENT_TESTS_MOCKSERVERPROCESS_H
#define WINSPIALERTCLIENT_TESTS_MOCKSERVERPROCESS_H

#include <string>

#include "HandlePtr.h"

namespace test_helpers
{

// Runs scripts/mock_server.py in a job object: the mock dies together with the test process, even on a crash
class MockServerProcess
{
public:
	MockServerProcess(const std::string& host, unsigned short port, const std::string& extra_args = {});
	~MockServerProcess();

	MockServerProcess(const MockServerProcess& other) = delete;
	MockServerProcess& operator=(const MockServerProcess& other) = delete;

	[[nodiscard]] bool is_listening() const noexcept;

	static unsigned short find_free_port(const std::string& host);

private:
	bool start(const std::string& extra_args);
	bool wait_until_listening() const;
	static bool can_connect(const std::string& host, unsigned short port);
	static HandlePtr open_log_file();

private:
	std::string _host;
	unsigned short _port;
	HandlePtr _job;
	HandlePtr _process;
	bool _listening{};
};

} // namespace test_helpers

#endif // WINSPIALERTCLIENT_TESTS_MOCKSERVERPROCESS_H
