#ifndef WINSPIALERTCLIENT_TESTS_MOCKSERVERENVIRONMENT_H
#define WINSPIALERTCLIENT_TESTS_MOCKSERVERENVIRONMENT_H

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "net/WinHttpClient.h"
#include "tests/helpers/MockServerProcess.h"

#define MOCK_HOST "127.0.0.1"
#define MOCK_API_KEY "integration-key"
#define MOCK_API_BASE_PATH "/api/v1"
#define TEST_USER_AGENT "WinSpiAlertClient-tests/1.0"

namespace integration
{

// One mock server for the whole run; every test resets its state (see MockServerTest)
class MockServerEnvironment : public testing::Environment
{
public:
	void SetUp() override;
	void TearDown() override;

	static unsigned short port();

private:
	static std::unique_ptr<test_helpers::MockServerProcess> _mock;
	static unsigned short _port;
};

class MockServerTest : public testing::Test
{
protected:
	void SetUp() override;

	// /__mock__ control API: no API key, outside the rate limit
	void add_fault(const std::string& fault_json);

	net::WinHttpClient _client{MOCK_HOST, TEST_USER_AGENT, MockServerEnvironment::port()};
};

} // namespace integration

#endif // WINSPIALERTCLIENT_TESTS_MOCKSERVERENVIRONMENT_H
