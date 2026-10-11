#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <windows.h>
#include <winhttp.h>

#include "api/SpialertApi.h"
#include "api/dto/StatusDto.h"
#include "net/IHttpClient.h"

using testing::_;
using testing::Return;

namespace
{

class MockHttpClient : public net::IHttpClient
{
public:
	MOCK_METHOD(bool, open, (), (override));
	MOCK_METHOD(bool, close, (), (override));
	MOCK_METHOD(net::HttpResponse, send_request,
	            (const std::string& path, const std::string& method, const std::string& body,
	             const std::string& headers),
	            (override));
};

net::HttpResponse http_response(unsigned long status_code, std::string body)
{
	net::HttpResponse response;
	response.status_code = status_code;
	response.body = std::move(body);
	return response;
}

net::HttpResponse network_error(unsigned long error_code)
{
	net::HttpResponse response;
	response.error_code = error_code;
	response.error_message = "network error";
	return response;
}
} // namespace

class SpialertApiTests : public testing::Test
{
protected:
	SpialertApiTests()
	{
		auto client = std::make_unique<testing::StrictMock<MockHttpClient>>();
		_http_client = client.get();
		_api = std::make_unique<api::SpialertApi>(std::move(client), "/api/v1", "test-key");
	}

	testing::StrictMock<MockHttpClient>* _http_client{};
	std::unique_ptr<api::SpialertApi> _api;
};

TEST_F(SpialertApiTests, GetStatusSendsGetWithApiKey)
{
	EXPECT_CALL(*_http_client, send_request("/api/v1/status", "GET", "", "X-API-Key: test-key"))
		.WillOnce(Return(http_response(200, R"({"status":"online"})")));

	_api->get_status();
}

TEST_F(SpialertApiTests, GetStatusParsesSuccessfulResponse)
{
	EXPECT_CALL(*_http_client, send_request(_, _, _, _))
		.WillOnce(
			Return(http_response(200, R"({"status":"online","api_version":"1.0","candidate":"Mock Candidate"})")));

	const auto response = _api->get_status();

	EXPECT_TRUE(response.is_success());
	EXPECT_EQ(response.status_code, 200);
	EXPECT_TRUE(response.dto.status);
	EXPECT_EQ(response.dto.api_version, "1.0");
	EXPECT_EQ(response.dto.candidate, "Mock Candidate");
}

TEST_F(SpialertApiTests, GetStatusReportsServerNotOnline)
{
	EXPECT_CALL(*_http_client, send_request(_, _, _, _))
		.WillOnce(Return(http_response(200, R"({"status":"maintenance"})")));

	const auto response = _api->get_status();

	EXPECT_TRUE(response.is_success());
	EXPECT_FALSE(response.dto.status);
}

TEST_F(SpialertApiTests, GetStatusDoesNotParseErrorBody)
{
	EXPECT_CALL(*_http_client, send_request(_, _, _, _))
		.WillOnce(Return(http_response(401, R"({"error":"invalid api key","status":"online"})")));

	const auto response = _api->get_status();

	EXPECT_FALSE(response.is_success());
	EXPECT_EQ(response.status_code, 401);
	EXPECT_FALSE(response.dto.status);
}

TEST_F(SpialertApiTests, GetStatusKeepsRateLimitStatusCode)
{
	auto rate_limited = http_response(429, R"({"error":"rate limit exceeded"})");
	rate_limited.retry_after = std::chrono::seconds{5};
	EXPECT_CALL(*_http_client, send_request(_, _, _, _)).WillOnce(Return(rate_limited));

	const auto response = _api->get_status();

	EXPECT_FALSE(response.is_success());
	EXPECT_EQ(response.status_code, 429);
}

TEST_F(SpialertApiTests, GetStatusNetworkErrorIsNotSuccess)
{
	EXPECT_CALL(*_http_client, send_request(_, _, _, _)).WillOnce(Return(network_error(ERROR_WINHTTP_TIMEOUT)));

	const auto response = _api->get_status();

	EXPECT_FALSE(response.is_success());
	EXPECT_EQ(response.status_code, 0);
	EXPECT_FALSE(response.dto.status);
}
