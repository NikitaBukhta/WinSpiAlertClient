#include <gtest/gtest.h>

#include "net/WinHttpClient.h"

// Only local checks: no requests leave the machine

#define TEST_USER_AGENT "WinSpiAlertClient-tests/1.0"
#define LOCAL_HOST "127.0.0.1"
#define CLOSED_PORT 1

using net::WinHttpClient;

TEST(WinHttpClientTests, OpenAndCloseSucceed)
{
	WinHttpClient client(LOCAL_HOST, TEST_USER_AGENT, CLOSED_PORT);

	EXPECT_TRUE(client.open());
	EXPECT_TRUE(client.close());
}

TEST(WinHttpClientTests, OpenTwiceSucceeds)
{
	WinHttpClient client(LOCAL_HOST, TEST_USER_AGENT, CLOSED_PORT);

	EXPECT_TRUE(client.open());
	EXPECT_TRUE(client.open());
}

TEST(WinHttpClientTests, SendBeforeOpenReturnsHandleStateError)
{
	WinHttpClient client(LOCAL_HOST, TEST_USER_AGENT, CLOSED_PORT);

	const auto response = client.send_request("/api/v1/status", "GET", {}, {});

	EXPECT_FALSE(response.has_response());
	EXPECT_EQ(response.error_code, ERROR_WINHTTP_INCORRECT_HANDLE_STATE);
	EXPECT_EQ(response.status_code, 0u);
	EXPECT_NE(response.error_message.find("12019"), std::string::npos) << response.error_message;
}

TEST(WinHttpClientTests, SendAfterCloseReturnsHandleStateError)
{
	WinHttpClient client(LOCAL_HOST, TEST_USER_AGENT, CLOSED_PORT);
	ASSERT_TRUE(client.open());
	client.close();

	const auto response = client.send_request("/api/v1/status", "GET", {}, {});

	EXPECT_EQ(response.error_code, ERROR_WINHTTP_INCORRECT_HANDLE_STATE);
}

TEST(WinHttpClientTests, ClosedPortReturnsCannotConnect)
{
	WinHttpClient client(LOCAL_HOST, TEST_USER_AGENT, CLOSED_PORT);
	ASSERT_TRUE(client.open());

	const auto response = client.send_request("/api/v1/status", "GET", {}, {});

	EXPECT_FALSE(response.has_response());
	EXPECT_EQ(response.error_code, ERROR_WINHTTP_CANNOT_CONNECT);
	EXPECT_EQ(response.status_code, 0u);
	EXPECT_NE(response.error_message.find("12029"), std::string::npos) << response.error_message;
}
