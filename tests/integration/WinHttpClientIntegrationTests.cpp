#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <string>

#include "MockServerEnvironment.h"
#include "api/dto/EventsListDto.h"
#include "tests/helpers/DtoParser.h"
#include "tests/helpers/MockServerProcess.h"

#define API_KEY_HEADER "X-API-Key: " MOCK_API_KEY
#define JSON_HEADERS API_KEY_HEADER "\r\nContent-Type: application/json"
#define STATUS_PATH MOCK_API_BASE_PATH "/status"
#define EVENTS_PATH MOCK_API_BASE_PATH "/events"
#define READ_CHUNK_SIZE (8 * 1024)
#define BULK_EVENT_COUNT 60
// a loopback address that is not in the mock certificate (it covers localhost and 127.0.0.1 only)
#define HOST_NOT_IN_CERTIFICATE "127.0.0.2"

using integration::MockServerTest;
using test_helpers::DtoParser;

namespace
{

class WinHttpClientIntegrationTests : public MockServerTest
{
};

std::string event_json(int number)
{
	char uid[32]{};
	std::snprintf(uid, sizeof(uid), "uid-%04d", number);
	return std::string(R"({"event_uid":")") + uid +
	       R"(","event_code":3,"file_name":"a.png","timestamp":"2026-10-11T12:30:45Z"})";
}

} // namespace

TEST_F(WinHttpClientIntegrationTests, GetReturnsStatusCodeAndBody)
{
	const auto response = _client.send_request(STATUS_PATH, "GET", {}, API_KEY_HEADER);

	EXPECT_TRUE(response.has_response()) << response.error_message;
	EXPECT_EQ(response.status_code, 200u);
	EXPECT_NE(response.body.find(R"("status": "online")"), std::string::npos) << response.body;
	EXPECT_FALSE(response.retry_after.has_value());
}

TEST_F(WinHttpClientIntegrationTests, SeveralRequestsReuseTheClient)
{
	for (int i = 0; i < 3; ++i)
	{
		const auto response = _client.send_request(STATUS_PATH, "GET", {}, API_KEY_HEADER);
		EXPECT_EQ(response.status_code, 200u) << "request " << i << ": " << response.error_message;
	}
}

TEST_F(WinHttpClientIntegrationTests, HttpErrorIsAResponseNotANetworkError)
{
	const auto response = _client.send_request(STATUS_PATH, "GET", {}, "X-API-Key: wrong-key");

	EXPECT_TRUE(response.has_response());
	EXPECT_EQ(response.status_code, 401u);
	EXPECT_NE(response.body.find("invalid_api_key"), std::string::npos) << response.body;
}

TEST_F(WinHttpClientIntegrationTests, UnknownPathReturnsNotFound)
{
	const auto response = _client.send_request(MOCK_API_BASE_PATH "/unknown", "GET", {}, API_KEY_HEADER);

	EXPECT_EQ(response.status_code, 404u);
}

TEST_F(WinHttpClientIntegrationTests, PostSendsBodyAndAllHeaders)
{
	const auto created = _client.send_request(EVENTS_PATH, "POST", event_json(1), JSON_HEADERS);
	const auto repeated = _client.send_request(EVENTS_PATH, "POST", event_json(1), JSON_HEADERS);

	EXPECT_EQ(created.status_code, 201u) << created.body;
	EXPECT_NE(created.body.find(R"("event_uid": "uid-0001")"), std::string::npos) << created.body;
	EXPECT_EQ(repeated.status_code, 200u) << "same event_uid must be idempotent: " << repeated.body;
}

TEST_F(WinHttpClientIntegrationTests, PostWithoutContentTypeIsRejectedByServer)
{
	const auto response = _client.send_request(EVENTS_PATH, "POST", event_json(1), API_KEY_HEADER);

	EXPECT_EQ(response.status_code, 415u) << response.body;
}

TEST_F(WinHttpClientIntegrationTests, ReadsBodyLargerThanOneChunk)
{
	for (int number = 1; number <= BULK_EVENT_COUNT; ++number)
		ASSERT_EQ(_client.send_request(EVENTS_PATH, "POST", event_json(number), JSON_HEADERS).status_code, 201u);

	const auto response = _client.send_request(EVENTS_PATH "?limit=100", "GET", {}, API_KEY_HEADER);
	ASSERT_EQ(response.status_code, 200u);
	ASSERT_GT(response.body.size(), static_cast<size_t>(READ_CHUNK_SIZE));

	const auto page = DtoParser::parse<api::dto::EventsListMapper>(response.body);
	EXPECT_EQ(page.total, BULK_EVENT_COUNT);
	ASSERT_EQ(page.items.size(), static_cast<size_t>(BULK_EVENT_COUNT));
	EXPECT_EQ(page.items.front().event_uid, "uid-0060");
	EXPECT_EQ(page.items.back().event_uid, "uid-0001");
}

TEST_F(WinHttpClientIntegrationTests, ParsesRetryAfterHeader)
{
	add_fault(R"({"fault":"status","status":429,"retry_after":7})");

	const auto response = _client.send_request(STATUS_PATH, "GET", {}, API_KEY_HEADER);

	EXPECT_EQ(response.status_code, 429u);
	ASSERT_TRUE(response.retry_after.has_value());
	EXPECT_EQ(*response.retry_after, std::chrono::seconds{7});
}

TEST_F(WinHttpClientIntegrationTests, NonJsonBodyIsReturnedAsIs)
{
	add_fault(R"({"fault":"html"})");

	const auto response = _client.send_request(STATUS_PATH, "GET", {}, API_KEY_HEADER);

	EXPECT_EQ(response.status_code, 502u);
	EXPECT_EQ(response.body.rfind("<html>", 0), 0u) << response.body;
}

TEST_F(WinHttpClientIntegrationTests, ConnectionResetIsANetworkError)
{
	add_fault(R"({"fault":"reset","count":5})");

	const auto response = _client.send_request(STATUS_PATH, "GET", {}, API_KEY_HEADER);

	EXPECT_FALSE(response.has_response());
	EXPECT_EQ(response.status_code, 0u);
	EXPECT_NE(response.error_code, 0u);
	EXPECT_FALSE(response.error_message.empty());
}

TEST_F(WinHttpClientIntegrationTests, BodyCutOffMidwayIsANetworkError)
{
	add_fault(R"({"fault":"truncate","count":5})");

	const auto response = _client.send_request(STATUS_PATH, "GET", {}, API_KEY_HEADER);

	EXPECT_FALSE(response.has_response());
	EXPECT_EQ(response.status_code, 0u) << "a partial body must not be reported as a successful response";
	EXPECT_TRUE(response.body.empty());
}

TEST_F(WinHttpClientIntegrationTests, LostResponseCanBeResentWithTheSameEventUid)
{
	add_fault(R"({"fault":"drop","method":"POST","path":")" EVENTS_PATH R"("})");

	const auto lost = _client.send_request(EVENTS_PATH, "POST", event_json(1), JSON_HEADERS);
	const auto resent = _client.send_request(EVENTS_PATH, "POST", event_json(1), JSON_HEADERS);

	EXPECT_FALSE(lost.has_response());
	EXPECT_EQ(resent.status_code, 200u) << "the server already has the event, resend must not create a duplicate";
	const auto list = _client.send_request(EVENTS_PATH, "GET", {}, API_KEY_HEADER);
	EXPECT_EQ(DtoParser::parse<api::dto::EventsListMapper>(list.body).total, 1);
}

TEST(WinHttpClientTlsTests, HostNotInCertificateIsRejected)
{
	const unsigned short port = test_helpers::MockServerProcess::find_free_port(HOST_NOT_IN_CERTIFICATE);
	const test_helpers::MockServerProcess mock(HOST_NOT_IN_CERTIFICATE, port);
	ASSERT_TRUE(mock.is_listening());
	net::WinHttpClient client(HOST_NOT_IN_CERTIFICATE, TEST_USER_AGENT, port);
	ASSERT_TRUE(client.open());

	const auto response = client.send_request(STATUS_PATH, "GET", {}, "X-API-Key: test-key");

	EXPECT_FALSE(response.has_response());
	EXPECT_EQ(response.error_code, static_cast<unsigned long>(ERROR_WINHTTP_SECURE_FAILURE));
}
