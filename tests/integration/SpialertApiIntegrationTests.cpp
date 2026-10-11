#include <gtest/gtest.h>

#include <chrono>
#include <memory>

#include "MockServerEnvironment.h"
#include "api/SpialertApi.h"
#include "api/dto/StatusDto.h"
#include "net/WinHttpClient.h"

#define SERVER_TIME_TOLERANCE std::chrono::minutes{1}

using integration::MockServerEnvironment;
using integration::MockServerTest;

namespace
{

class SpialertApiIntegrationTests : public MockServerTest
{
};

std::unique_ptr<api::SpialertApi> make_api(const std::string& api_key)
{
	auto http_client = std::make_unique<net::WinHttpClient>(MOCK_HOST, TEST_USER_AGENT, MockServerEnvironment::port());
	if (!http_client->open())
		return nullptr;
	return std::make_unique<api::SpialertApi>(std::move(http_client), MOCK_API_BASE_PATH, api_key);
}

} // namespace

TEST_F(SpialertApiIntegrationTests, GetStatusReportsOnlineServer)
{
	const auto spialert_api = make_api(MOCK_API_KEY);
	ASSERT_NE(spialert_api, nullptr);

	const auto response = spialert_api->get_status();

	ASSERT_TRUE(response.is_success()) << "HTTP " << response.status_code;
	EXPECT_TRUE(response.dto.status);
	EXPECT_EQ(response.dto.api_version, "1.0");
	EXPECT_EQ(response.dto.candidate, "Mock Candidate");
	const auto clock_skew = std::chrono::abs(std::chrono::system_clock::now() - response.dto.server_time);
	EXPECT_LT(clock_skew, SERVER_TIME_TOLERANCE);
}

TEST_F(SpialertApiIntegrationTests, GetStatusWithWrongKeyIsNotSuccess)
{
	const auto spialert_api = make_api("wrong-key");
	ASSERT_NE(spialert_api, nullptr);

	const auto response = spialert_api->get_status();

	EXPECT_FALSE(response.is_success());
	EXPECT_EQ(response.status_code, 401);
	EXPECT_FALSE(response.dto.status);
}

TEST_F(SpialertApiIntegrationTests, GetStatusServerErrorIsNotSuccess)
{
	add_fault(R"({"fault":"status","status":503})");
	const auto spialert_api = make_api(MOCK_API_KEY);
	ASSERT_NE(spialert_api, nullptr);

	const auto response = spialert_api->get_status();

	EXPECT_FALSE(response.is_success());
	EXPECT_EQ(response.status_code, 503);
}

TEST_F(SpialertApiIntegrationTests, GetStatusNetworkErrorIsNotSuccess)
{
	add_fault(R"({"fault":"reset","count":5})");
	const auto spialert_api = make_api(MOCK_API_KEY);
	ASSERT_NE(spialert_api, nullptr);

	const auto response = spialert_api->get_status();

	EXPECT_FALSE(response.is_success());
	EXPECT_EQ(response.status_code, 0);
}
