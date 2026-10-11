#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "MockServerEnvironment.h"
#include "api/SpialertApi.h"
#include "api/dto/EventsListDto.h"
#include "app/SpialertService.h"
#include "net/WinHttpClient.h"
#include "tests/helpers/TempFile.h"

#define PNG_IMAGE "\x89PNG\r\n\x1a\nimage-bytes"
#define DEFAULT_LIMIT 20

using integration::MockServerEnvironment;
using integration::MockServerTest;

namespace
{

class SpialertServiceIntegrationTests : public MockServerTest
{
protected:
	void SetUp() override
	{
		MockServerTest::SetUp();
		_service = std::make_unique<app::SpialertService>(make_api());
		_events_api = make_api();
	}

	static std::unique_ptr<api::SpialertApi> make_api()
	{
		auto http_client =
			std::make_unique<net::WinHttpClient>(MOCK_HOST, TEST_USER_AGENT, MockServerEnvironment::port());
		return std::make_unique<api::SpialertApi>(std::move(http_client), MOCK_API_BASE_PATH, MOCK_API_KEY);
	}

	api::dto::EventsListDto list_events()
	{
		return _events_api->get_events(DEFAULT_LIMIT, 0, std::nullopt).dto;
	}

	std::unique_ptr<app::SpialertService> _service;
	std::unique_ptr<api::SpialertApi> _events_api;
};

} // namespace

TEST_F(SpialertServiceIntegrationTests, MockServerIsOnline)
{
	EXPECT_TRUE(_service->is_online());
}

TEST_F(SpialertServiceIntegrationTests, SendImgCreatesEventWithUploadedImage)
{
	const test_helpers::TempFile image("capture_0001.png", PNG_IMAGE);

	ASSERT_TRUE(_service->send_img(image.path()));

	const auto events = list_events();
	ASSERT_EQ(events.items.size(), 1u);
	EXPECT_EQ(events.items[0].file_name, "capture_0001.png");
	EXPECT_TRUE(events.items[0].image_uploaded);
	EXPECT_EQ(events.items[0].image_size, std::string(PNG_IMAGE).size());
}

TEST_F(SpialertServiceIntegrationTests, SendImgSurvivesLostResponseWithoutDuplicate)
{
	add_fault(R"({"fault":"drop","method":"POST","path":"/api/v1/events"})");
	const test_helpers::TempFile image("capture_0001.png", PNG_IMAGE);

	ASSERT_TRUE(_service->send_img(image.path()));

	EXPECT_EQ(list_events().total, 1) << "the resent event must be the same one, not a second event";
}

TEST_F(SpialertServiceIntegrationTests, SendImgSurvivesLostUploadResponse)
{
	// the server stores the image but the response is lost: the resend gets 409 image_already_uploaded
	add_fault(R"({"fault":"drop","method":"POST","path":"/api/v1/events/*/image"})");
	const test_helpers::TempFile image("capture_0001.png", PNG_IMAGE);

	ASSERT_TRUE(_service->send_img(image.path()));

	const auto events = list_events();
	ASSERT_EQ(events.items.size(), 1u);
	EXPECT_TRUE(events.items[0].image_uploaded);
}

TEST_F(SpialertServiceIntegrationTests, SendImgSurvivesUploadConnectionReset)
{
	// the upload never reaches the server: the resend uploads it
	add_fault(R"({"fault":"reset","method":"POST","path":"/api/v1/events/*/image"})");
	const test_helpers::TempFile image("capture_0001.png", PNG_IMAGE);

	ASSERT_TRUE(_service->send_img(image.path()));

	EXPECT_TRUE(list_events().items.at(0).image_uploaded);
}

TEST_F(SpialertServiceIntegrationTests, SendImgRejectedFileNameCreatesNoUpload)
{
	const test_helpers::TempFile image("capture 0001.png", PNG_IMAGE);

	EXPECT_FALSE(_service->send_img(image.path()));

	EXPECT_EQ(list_events().total, 0) << "the name breaks the server rules, so the client sends nothing";
}
