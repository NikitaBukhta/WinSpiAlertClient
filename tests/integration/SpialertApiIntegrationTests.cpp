#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

#include "MockServerEnvironment.h"
#include "api/ApiConstants.h"
#include "api/SpialertApi.h"
#include "api/dto/EventObjectDto.h"
#include "api/dto/EventRequestDto.h"
#include "api/dto/EventsListDto.h"
#include "api/dto/StatusDto.h"
#include "model/Image.h"
#include "net/WinHttpClient.h"
#include "utils/UuidGenerator.h"

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
	return std::make_unique<api::SpialertApi>(std::move(http_client), MOCK_API_BASE_PATH, api_key);
}

api::dto::EventRequestDto make_event(const std::string& file_name)
{
	api::dto::EventRequestDto event;
	event.event_uid = utils::UuidGenerator::generate();
	event.event_code = 3;
	event.file_name = file_name;
	event.timestamp = std::chrono::system_clock::now();
	return event;
}

} // namespace

TEST_F(SpialertApiIntegrationTests, GetStatusReportsOnlineServer)
{
	const auto spialert_api = make_api(MOCK_API_KEY);

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

	const auto response = spialert_api->get_status();

	EXPECT_FALSE(response.is_success());
	EXPECT_EQ(response.status_code, 401);
	EXPECT_FALSE(response.dto.status);
}

TEST_F(SpialertApiIntegrationTests, GetStatusServerErrorIsNotSuccess)
{
	add_fault(R"({"fault":"status","status":503})");
	const auto spialert_api = make_api(MOCK_API_KEY);

	const auto response = spialert_api->get_status();

	EXPECT_FALSE(response.is_success());
	EXPECT_EQ(response.status_code, 503);
}

TEST_F(SpialertApiIntegrationTests, GetStatusNetworkErrorIsNotSuccess)
{
	add_fault(R"({"fault":"reset","count":5})");
	const auto spialert_api = make_api(MOCK_API_KEY);

	const auto response = spialert_api->get_status();

	EXPECT_FALSE(response.is_success());
	EXPECT_EQ(response.status_code, 0);
}

TEST_F(SpialertApiIntegrationTests, PostEventsCreatesEventAndResendIsIdempotent)
{
	const auto spialert_api = make_api(MOCK_API_KEY);
	const auto event = make_event("a.png");

	const auto created = spialert_api->post_events(event);
	const auto resent = spialert_api->post_events(event);

	ASSERT_EQ(created.status_code, 201);
	EXPECT_GT(created.dto.event_id, 0);
	EXPECT_EQ(created.dto.event_uid, event.event_uid);
	EXPECT_EQ(created.dto.file_name, "a.png");
	EXPECT_FALSE(created.dto.image_uploaded);
	EXPECT_EQ(resent.status_code, 200);
	EXPECT_EQ(resent.dto.event_id, created.dto.event_id);
}

TEST_F(SpialertApiIntegrationTests, ResendAfterLostResponseDoesNotDuplicateEvent)
{
	add_fault(R"({"fault":"drop","method":"POST","path":"/api/v1/events"})");
	const auto spialert_api = make_api(MOCK_API_KEY);
	const auto event = make_event("a.png");

	const auto lost = spialert_api->post_events(event);
	const auto resent = spialert_api->post_events(event);

	EXPECT_EQ(lost.status_code, 0);
	EXPECT_EQ(resent.status_code, 200) << "the first request reached the server, the resend must find it";
	EXPECT_EQ(spialert_api->get_events(DEFAULT_EVENTS_LIMIT, 0, std::nullopt).dto.total, 1);
}

TEST_F(SpialertApiIntegrationTests, PostImageUploadsTheRegisteredFile)
{
	const auto spialert_api = make_api(MOCK_API_KEY);
	const auto created = spialert_api->post_events(make_event("capture_0001.png"));
	ASSERT_EQ(created.status_code, 201);
	const model::Image image("capture_0001.png", PNG_IMAGE);

	const auto uploaded = spialert_api->post_image(image, created.dto.event_id);

	ASSERT_EQ(uploaded.status_code, 200);
	EXPECT_TRUE(uploaded.dto.image_uploaded);
	EXPECT_EQ(uploaded.dto.image_size, std::string(PNG_IMAGE).size());
	EXPECT_EQ(uploaded.dto.image_sha256.size(), 64u);
}

TEST_F(SpialertApiIntegrationTests, PostImageWithOtherFileNameIsRejected)
{
	const auto spialert_api = make_api(MOCK_API_KEY);
	const auto created = spialert_api->post_events(make_event("a.png"));
	const model::Image image("b.png", PNG_IMAGE);

	const auto uploaded = spialert_api->post_image(image, created.dto.event_id);

	EXPECT_EQ(uploaded.status_code, 422);
	EXPECT_FALSE(uploaded.dto.image_uploaded);
}

TEST_F(SpialertApiIntegrationTests, GetEventsFiltersByImageUploaded)
{
	const auto spialert_api = make_api(MOCK_API_KEY);
	const auto with_image = spialert_api->post_events(make_event("a.png"));
	const auto without_image = spialert_api->post_events(make_event("b.png"));
	const model::Image image("a.png", PNG_IMAGE);
	ASSERT_EQ(spialert_api->post_image(image, with_image.dto.event_id).status_code, 200);

	const auto pending = spialert_api->get_events(DEFAULT_EVENTS_LIMIT, 0, false);
	const auto uploaded = spialert_api->get_events(DEFAULT_EVENTS_LIMIT, 0, true);

	ASSERT_TRUE(pending.is_success());
	ASSERT_EQ(pending.dto.items.size(), 1u);
	EXPECT_EQ(pending.dto.items[0].event_id, without_image.dto.event_id);
	ASSERT_EQ(uploaded.dto.items.size(), 1u);
	EXPECT_EQ(uploaded.dto.items[0].event_id, with_image.dto.event_id);
}

TEST_F(SpialertApiIntegrationTests, GetEventsPagesNewestFirst)
{
	const auto spialert_api = make_api(MOCK_API_KEY);
	for (int i = 0; i < 3; ++i)
		ASSERT_EQ(spialert_api->post_events(make_event("a.png")).status_code, 201);

	const auto page = spialert_api->get_events(2, 1, std::nullopt);

	ASSERT_TRUE(page.is_success());
	EXPECT_EQ(page.dto.total, 3);
	EXPECT_EQ(page.dto.limit, 2);
	EXPECT_EQ(page.dto.offset, 1);
	ASSERT_EQ(page.dto.items.size(), 2u);
	EXPECT_GT(page.dto.items[0].event_id, page.dto.items[1].event_id);
}
