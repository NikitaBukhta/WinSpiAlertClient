#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <rapidjson/document.h>

#include "MockServerEnvironment.h"
#include "api/SpialertApi.h"
#include "api/dto/EventsListDto.h"
#include "app/SpialertService.h"
#include "model/WicImageCompressorFactory.h"
#include "net/WinHttpClient.h"
#include "tests/helpers/ImageCodec.h"
#include "tests/helpers/TempFile.h"
#include "utils/FileUtils.h"

#define PNG_IMAGE "\x89PNG\r\n\x1a\nimage-bytes"
#define DEFAULT_LIMIT 20
#define LARGE_JPEG_WIDTH 4000
#define LARGE_JPEG_HEIGHT 3000
#define MAX_MANIFEST_SIZE (64 * 1024)

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
		_service =
			std::make_unique<app::SpialertService>(make_api(), std::make_unique<model::WicImageCompressorFactory>());
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

struct RealImage
{
	std::string name;
	bool sent{};
};

// images from tests/data/images.json, downloaded into TEST_IMAGES_DIR by scripts/project.py
std::vector<RealImage> read_real_images()
{
	const auto json = utils::FileUtils::read_file(TEST_IMAGES_MANIFEST, MAX_MANIFEST_SIZE);
	rapidjson::Document manifest;
	if (!json || manifest.Parse(json->c_str()).HasParseError())
		return {};

	std::vector<RealImage> images;
	for (const auto& image : manifest["images"].GetArray())
		images.push_back({image["name"].GetString(), image["sent"].GetBool()});
	return images;
}

std::string test_name(const testing::TestParamInfo<RealImage>& info)
{
	std::string name = info.param.name;
	std::ranges::replace(name, '.', '_');
	return name;
}

class RealImageIntegrationTests : public SpialertServiceIntegrationTests, public testing::WithParamInterface<RealImage>
{
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

TEST_F(SpialertServiceIntegrationTests, SendImgUploadsLargeJpegShrunkUnderTheServerLimit)
{
	const std::string jpeg = test_helpers::ImageCodec().noise(GUID_ContainerFormatJpeg, GUID_WICPixelFormat24bppBGR,
	                                                          LARGE_JPEG_WIDTH, LARGE_JPEG_HEIGHT);
	ASSERT_GT(jpeg.size(), 4 * MAX_IMAGE_SIZE);
	const test_helpers::TempFile image("capture_0001.jpg", jpeg);

	ASSERT_TRUE(_service->send_img(image.path()));

	const auto events = list_events();
	ASSERT_EQ(events.items.size(), 1u);
	EXPECT_EQ(events.items[0].file_name, "capture_0001.jpg");
	EXPECT_TRUE(events.items[0].image_uploaded);
	EXPECT_LE(events.items[0].image_size, MAX_IMAGE_SIZE);
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

TEST_P(RealImageIntegrationTests, SendImgShrinksOrRejectsRealImage)
{
	const auto& [name, sent] = GetParam();
	const auto path = std::filesystem::path(TEST_IMAGES_DIR) / name;
	ASSERT_TRUE(std::filesystem::exists(path))
	    << path << " is missing: py scripts\\project.py integration downloads it";

	EXPECT_EQ(_service->send_img(path), sent);

	const auto events = list_events();
	if (!sent)
	{
		EXPECT_EQ(events.total, 0);
		return;
	}
	ASSERT_EQ(events.items.size(), 1u);
	EXPECT_EQ(events.items[0].file_name, name);
	EXPECT_TRUE(events.items[0].image_uploaded);
	EXPECT_LE(events.items[0].image_size, MAX_IMAGE_SIZE);
}

INSTANTIATE_TEST_SUITE_P(RealImages, RealImageIntegrationTests, testing::ValuesIn(read_real_images()), test_name);
