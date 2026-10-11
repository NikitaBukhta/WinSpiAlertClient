#include <gtest/gtest.h>

#include "net/WinHttpClient.h"

// Only local checks: no requests leave the machine

#define TEST_USER_AGENT "WinSpiAlertClient-tests/1.0"
#define LOCAL_HOST "127.0.0.1"
#define CLOSED_PORT 1

using net::WinHttpClient;

TEST(WinHttpClientTests, FirstRequestOpensTheConnection)
{
	WinHttpClient client(LOCAL_HOST, TEST_USER_AGENT, CLOSED_PORT);
	const auto response = client.send_request("/api/v1/status", "GET", {}, {});

	EXPECT_EQ(response.error_code, ERROR_WINHTTP_CANNOT_CONNECT) << response.message;
}

TEST(WinHttpClientTests, FailedOpenReportsTheRealReason)
{
	WinHttpClient client("", TEST_USER_AGENT, CLOSED_PORT);

	const auto response = client.send_request("/api/v1/status", "GET", {}, {});

	EXPECT_FALSE(response.has_response());
	EXPECT_NE(response.error_code, 0u);
	EXPECT_NE(response.error_code, static_cast<uint32_t>(ERROR_WINHTTP_INCORRECT_HANDLE_STATE));
}

TEST(WinHttpClientTests, ClosedPortReturnsCannotConnect)
{
	WinHttpClient client(LOCAL_HOST, TEST_USER_AGENT, CLOSED_PORT);

	const auto response = client.send_request("/api/v1/status", "GET", {}, {});

	EXPECT_FALSE(response.has_response());
	EXPECT_EQ(response.error_code, ERROR_WINHTTP_CANNOT_CONNECT);
	EXPECT_EQ(response.status_code, 0u);
	EXPECT_NE(response.message.find("12029"), std::string::npos) << response.message;
}

TEST(WinHttpClientTests, SeveralRequestsReuseTheOpenedConnection)
{
	WinHttpClient client(LOCAL_HOST, TEST_USER_AGENT, CLOSED_PORT);

	const auto first = client.send_request("/api/v1/status", "GET", {}, {});
	const auto second = client.send_request("/api/v1/status", "GET", {}, {});

	EXPECT_EQ(first.error_code, ERROR_WINHTTP_CANNOT_CONNECT);
	EXPECT_EQ(second.error_code, ERROR_WINHTTP_CANNOT_CONNECT);
}
