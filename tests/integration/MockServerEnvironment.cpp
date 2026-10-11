#include "MockServerEnvironment.h"

#include "tests/helpers/RequestBody.h"

// high enough that bulk requests in one test never hit it; 429 is tested through an injected fault
#define MOCK_ARGS "--api-key " MOCK_API_KEY " --rate-limit 100000"

namespace integration
{

std::unique_ptr<test_helpers::MockServerProcess> MockServerEnvironment::_mock;
unsigned short MockServerEnvironment::_port{};

void MockServerEnvironment::SetUp()
{
	_port = test_helpers::MockServerProcess::find_free_port(MOCK_HOST);
	ASSERT_NE(_port, 0) << "no free port on " MOCK_HOST;

	_mock = std::make_unique<test_helpers::MockServerProcess>(MOCK_HOST, _port, MOCK_ARGS);
	ASSERT_TRUE(_mock->is_listening()) << "mock server did not start, see " MOCK_SERVER_LOG
										  ". Run 'py scripts\\project.py configure' once to create .venv";

	net::WinHttpClient client(MOCK_HOST, TEST_USER_AGENT, _port);
	const net::HttpResponse probe = client.send_request("/__mock__/state", "GET", {}, {});
	ASSERT_NE(probe.error_code, static_cast<uint32_t>(ERROR_WINHTTP_SECURE_FAILURE))
	    << "the mock certificate is not trusted: run 'py scripts\\mock_server.py install-cert' once";
	ASSERT_EQ(probe.status_code, 200u) << probe.message;
}

void MockServerEnvironment::TearDown()
{
	_mock.reset();
}

unsigned short MockServerEnvironment::port()
{
	return _port;
}

void MockServerTest::SetUp()
{
	const net::HttpResponse reset = _client.send_request("/__mock__/reset", "POST", {}, {});
	ASSERT_EQ(reset.status_code, 200u) << reset.message;
}

void MockServerTest::add_fault(const std::string& fault_json)
{
	const net::HttpResponse added =
		_client.send_request("/__mock__/faults", "POST", test_helpers::RequestBody::from_string(fault_json), {});
	ASSERT_EQ(added.status_code, 200u) << added.body << added.message;
}

} // namespace integration
