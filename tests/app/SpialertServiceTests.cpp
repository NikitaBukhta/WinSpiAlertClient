#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "api/ISpialertApi.h"
#include "api/dto/EventObjectDto.h"
#include "api/dto/EventRequestDto.h"
#include "api/dto/EventsListDto.h"
#include "api/dto/StatusDto.h"
#include "app/SpialertService.h"
#include "model/IImageCompressor.h"
#include "model/IImageCompressorFactory.h"
#include "model/Image.h"
#include "model/WicImageCompressorFactory.h"
#include "tests/helpers/ImageCodec.h"
#include "tests/helpers/TempFile.h"

#define EVENT_ID 42
#define SEND_COUNT 50
#define TIMESTAMP_TOLERANCE std::chrono::seconds{5}
#define IMAGE_DATA "\x89PNG\r\n\x1a\nimage"
#define MB (1024 * 1024)
#define LARGE_SIDE 4000u

using test_helpers::ImageCodec;
using test_helpers::TempFile;
using testing::_;
using testing::AllOf;
using testing::ByMove;
using testing::Le;
using testing::Property;
using testing::Return;

namespace
{

class MockImageCompressor : public model::IImageCompressor
{
public:
	MOCK_METHOD(uint32_t, min_side, (), (const, override));
	MOCK_METHOD(double, decoded_pixel_ratio, (), (const, override));
	MOCK_METHOD(std::optional<model::Image>, compress, (double scale, float quality), (const, override));
};

class MockImageCompressorFactory : public model::IImageCompressorFactory
{
public:
	MOCK_METHOD(std::unique_ptr<model::IImageCompressor>, create, (const model::Image& image), (const, override));
};

class MockSpialertApi : public api::ISpialertApi
{
public:
	MOCK_METHOD(api::ApiResponse<api::dto::StatusDto>, get_status, (), (override));
	MOCK_METHOD(api::ApiResponse<api::dto::EventObjectDto>, post_events, (const api::dto::EventRequestDto& event),
	            (override));
	MOCK_METHOD(api::ApiResponse<api::dto::EventsListDto>, get_events,
	            (int limit, int offset, std::optional<bool> image_uploaded), (override));
	MOCK_METHOD(api::ApiResponse<api::dto::EventObjectDto>, post_image, (const model::Image& image, int event_id),
	            (override));
};

// a PNG of the given size as a compressor returns it
model::Image compressed_png(size_t size)
{
	const std::string signature = IMAGE_DATA;
	return model::Image("capture_0001.png", signature + std::string(size - signature.size(), 'x'));
}

api::ApiResponse<api::dto::StatusDto> status_response(int status_code, bool online)
{
	api::ApiResponse<api::dto::StatusDto> response{};
	response.status_code = status_code;
	response.dto.status = online;
	return response;
}

api::ApiResponse<api::dto::EventObjectDto> event_response(int status_code, bool image_uploaded = false)
{
	api::ApiResponse<api::dto::EventObjectDto> response{};
	response.status_code = status_code;
	if (response.is_success())
	{
		response.dto.event_id = EVENT_ID;
		response.dto.image_uploaded = image_uploaded;
	}
	return response;
}

class SpialertServiceTests : public testing::Test
{
protected:
	SpialertServiceTests()
	{
		auto spialert_api = std::make_unique<testing::StrictMock<MockSpialertApi>>();
		auto compressor_factory = std::make_unique<testing::StrictMock<MockImageCompressorFactory>>();
		_spialert_api = spialert_api.get();
		_compressor_factory = compressor_factory.get();
		_service = std::make_unique<app::SpialertService>(std::move(spialert_api), std::move(compressor_factory));
	}

	// the compressor the factory creates for the next image
	testing::StrictMock<MockImageCompressor>& expect_compressor()
	{
		auto compressor = std::make_unique<testing::StrictMock<MockImageCompressor>>();
		auto& reference = *compressor;
		EXPECT_CALL(reference, min_side()).WillRepeatedly(Return(LARGE_SIDE));
		EXPECT_CALL(reference, decoded_pixel_ratio()).WillRepeatedly(Return(1.0));
		std::unique_ptr<model::IImageCompressor> created = std::move(compressor);
		EXPECT_CALL(*_compressor_factory, create(_)).WillOnce(Return(ByMove(std::move(created))));
		return reference;
	}

	testing::StrictMock<MockSpialertApi>* _spialert_api{};
	testing::StrictMock<MockImageCompressorFactory>* _compressor_factory{};
	std::unique_ptr<app::SpialertService> _service;
	const TempFile _image{"capture_0001.png", IMAGE_DATA};
};

} // namespace

TEST_F(SpialertServiceTests, OnlineServerIsOnline)
{
	EXPECT_CALL(*_spialert_api, get_status()).WillOnce(Return(status_response(200, true)));

	EXPECT_TRUE(_service->is_online());
}

TEST_F(SpialertServiceTests, ServerInMaintenanceIsNotOnline)
{
	EXPECT_CALL(*_spialert_api, get_status()).WillOnce(Return(status_response(200, false)));

	EXPECT_FALSE(_service->is_online());
}

TEST_F(SpialertServiceTests, HttpErrorIsNotOnline)
{
	EXPECT_CALL(*_spialert_api, get_status()).WillOnce(Return(status_response(401, false)));

	EXPECT_FALSE(_service->is_online());
}

TEST_F(SpialertServiceTests, NetworkErrorIsNotOnline)
{
	EXPECT_CALL(*_spialert_api, get_status()).WillOnce(Return(status_response(0, false)));

	EXPECT_FALSE(_service->is_online());
}

TEST_F(SpialertServiceTests, SendImgCreatesEventForTheFileThenUploadsIt)
{
	api::dto::EventRequestDto sent;
	EXPECT_CALL(*_spialert_api, post_events(_))
		.WillOnce(testing::DoAll(testing::SaveArg<0>(&sent), Return(event_response(201))));
	const auto loaded_image =
		AllOf(Property(&model::Image::file_name, "capture_0001.png"), Property(&model::Image::data, IMAGE_DATA));
	EXPECT_CALL(*_spialert_api, post_image(loaded_image, EVENT_ID)).WillOnce(Return(event_response(200, true)));

	EXPECT_TRUE(_service->send_img(_image.path()));

	EXPECT_EQ(sent.file_name, "capture_0001.png");
	EXPECT_FALSE(sent.event_uid.empty());
	EXPECT_GE(sent.event_code, 1);
	EXPECT_LE(sent.event_code, 10);
	EXPECT_LT(std::chrono::abs(std::chrono::system_clock::now() - sent.timestamp), TIMESTAMP_TOLERANCE);
}

TEST_F(SpialertServiceTests, SendImgResendsTheSameEventAfterNetworkError)
{
	std::vector<api::dto::EventRequestDto> sent;
	EXPECT_CALL(*_spialert_api, post_events(_))
		.WillOnce(testing::DoAll(testing::Invoke([&](const auto& event) { sent.push_back(event); }),
		                         Return(event_response(0))))
		.WillOnce(testing::DoAll(testing::Invoke([&](const auto& event) { sent.push_back(event); }),
		                         Return(event_response(200))));
	EXPECT_CALL(*_spialert_api, post_image(_, EVENT_ID)).WillOnce(Return(event_response(200, true)));

	EXPECT_TRUE(_service->send_img(_image.path()));

	ASSERT_EQ(sent.size(), 2u);
	EXPECT_EQ(sent[1].event_uid, sent[0].event_uid);
	EXPECT_EQ(sent[1].event_code, sent[0].event_code);
	EXPECT_EQ(sent[1].timestamp, sent[0].timestamp);
}

TEST_F(SpialertServiceTests, SendImgGivesUpAfterThreeNetworkErrors)
{
	EXPECT_CALL(*_spialert_api, post_events(_)).Times(3).WillRepeatedly(Return(event_response(0)));

	EXPECT_FALSE(_service->send_img(_image.path()));
}

TEST_F(SpialertServiceTests, SendImgDoesNotResendOnHttpError)
{
	EXPECT_CALL(*_spialert_api, post_events(_)).WillOnce(Return(event_response(409)));

	EXPECT_FALSE(_service->send_img(_image.path()));
}

TEST_F(SpialertServiceTests, SendImgReportsFailedUpload)
{
	EXPECT_CALL(*_spialert_api, post_events(_)).WillOnce(Return(event_response(201)));
	EXPECT_CALL(*_spialert_api, post_image(_, EVENT_ID)).WillOnce(Return(event_response(422)));

	EXPECT_FALSE(_service->send_img(_image.path()));
}

TEST_F(SpialertServiceTests, SendImgResendsTheImageAfterNetworkError)
{
	EXPECT_CALL(*_spialert_api, post_events(_)).WillOnce(Return(event_response(201)));
	EXPECT_CALL(*_spialert_api, post_image(_, EVENT_ID))
		.WillOnce(Return(event_response(0)))
		.WillOnce(Return(event_response(200, true)));

	EXPECT_TRUE(_service->send_img(_image.path()));
}

TEST_F(SpialertServiceTests, SendImgConflictAfterLostUploadIsSuccess)
{
	// the lost attempt reached the server, so the resend gets 409 image_already_uploaded
	EXPECT_CALL(*_spialert_api, post_events(_)).WillOnce(Return(event_response(201)));
	EXPECT_CALL(*_spialert_api, post_image(_, EVENT_ID))
		.WillOnce(Return(event_response(0)))
		.WillOnce(Return(event_response(409)));

	EXPECT_TRUE(_service->send_img(_image.path()));
}

TEST_F(SpialertServiceTests, SendImgConflictOnFirstUploadIsFailure)
{
	EXPECT_CALL(*_spialert_api, post_events(_)).WillOnce(Return(event_response(201)));
	EXPECT_CALL(*_spialert_api, post_image(_, EVENT_ID)).WillOnce(Return(event_response(409)));

	EXPECT_FALSE(_service->send_img(_image.path()));
}

TEST_F(SpialertServiceTests, SendImgGivesUpUploadAfterThreeNetworkErrors)
{
	EXPECT_CALL(*_spialert_api, post_events(_)).WillOnce(Return(event_response(201)));
	EXPECT_CALL(*_spialert_api, post_image(_, EVENT_ID)).Times(3).WillRepeatedly(Return(event_response(0)));

	EXPECT_FALSE(_service->send_img(_image.path()));
}

TEST_F(SpialertServiceTests, SendImgUploadNotMarkedAsUploadedIsFailure)
{
	EXPECT_CALL(*_spialert_api, post_events(_)).WillOnce(Return(event_response(201)));
	EXPECT_CALL(*_spialert_api, post_image(_, EVENT_ID)).WillOnce(Return(event_response(200, false)));

	EXPECT_FALSE(_service->send_img(_image.path()));
}

TEST_F(SpialertServiceTests, SendImgFileOverTheReadLimitCreatesNoEvent)
{
	// over MAX_IMAGE_SIZE + COMPRESSION_HEADROOM (20 MB): not even read
	const TempFile huge("huge.png", IMAGE_DATA + std::string(MAX_IMAGE_SIZE + 20 * 1024 * 1024, 'x'));

	EXPECT_FALSE(_service->send_img(huge.path()));
}

TEST_F(SpialertServiceTests, SendImgMissingFileCreatesNoEvent)
{
	EXPECT_FALSE(_service->send_img("Z:\\no\\such\\capture.png"));
}

TEST_F(SpialertServiceTests, SendImgUndecodableFileOverFiveMegabytesCreatesNoEvent)
{
	const TempFile large("capture_0001.png", IMAGE_DATA + std::string(MAX_IMAGE_SIZE, 'x'));
	EXPECT_CALL(*_compressor_factory, create(_)).WillOnce([](const model::Image&) {
		return std::unique_ptr<model::IImageCompressor>();
	});

	EXPECT_FALSE(_service->send_img(large.path()));
}

TEST_F(SpialertServiceTests, SendImgOverFiveMegabytesSendsTheFirstFittingAttempt)
{
	// the first attempt is too large, the second one fits: no more attempts
	const TempFile large("capture_0001.png", IMAGE_DATA + std::string(MAX_IMAGE_SIZE, 'x'));
	auto& compressor = expect_compressor();
	EXPECT_CALL(compressor, compress(_, _))
		.WillOnce(Return(compressed_png(6 * MB)))
		.WillOnce(Return(compressed_png(2 * MB)));
	EXPECT_CALL(*_spialert_api, post_events(testing::Field(&api::dto::EventRequestDto::file_name, "capture_0001.png")))
		.WillOnce(Return(event_response(201)));
	EXPECT_CALL(*_spialert_api, post_image(Property(&model::Image::size, 2 * MB), EVENT_ID))
		.WillOnce(Return(event_response(200, true)));

	EXPECT_TRUE(_service->send_img(large.path()));
}

TEST_F(SpialertServiceTests, SendImgThatNeverFitsCreatesNoEvent)
{
	const TempFile large("capture_0001.png", IMAGE_DATA + std::string(MAX_IMAGE_SIZE, 'x'));
	auto& compressor = expect_compressor();
	EXPECT_CALL(compressor, compress(_, _))
		.Times(testing::Between(1, 4))
		.WillRepeatedly(Return(compressed_png(6 * MB)));

	EXPECT_FALSE(_service->send_img(large.path()));
}

TEST_F(SpialertServiceTests, SendImgThatCannotBeReencodedCreatesNoEvent)
{
	const TempFile large("capture_0001.png", IMAGE_DATA + std::string(MAX_IMAGE_SIZE, 'x'));
	auto& compressor = expect_compressor();
	EXPECT_CALL(compressor, compress(_, _)).WillOnce(Return(std::nullopt));

	EXPECT_FALSE(_service->send_img(large.path()));
}

TEST(SpialertServiceWicTests, SendImgTwentyMegabyteJpegIsShrunkAndSent)
{
	auto spialert_api = std::make_unique<testing::StrictMock<MockSpialertApi>>();
	auto& api = *spialert_api;
	app::SpialertService service(std::move(spialert_api), std::make_unique<model::WicImageCompressorFactory>());
	const std::string jpeg = ImageCodec().noise(GUID_ContainerFormatJpeg, GUID_WICPixelFormat24bppBGR, 4000, 3000);
	ASSERT_GT(jpeg.size(), 4 * MAX_IMAGE_SIZE);
	const TempFile large("capture_0001.jpg", jpeg);
	EXPECT_CALL(api, post_events(_)).WillOnce(Return(event_response(201)));
	const auto shrunk = AllOf(Property(&model::Image::file_name, "capture_0001.jpg"),
	                          Property(&model::Image::size, Le(MAX_IMAGE_SIZE)));
	EXPECT_CALL(api, post_image(shrunk, EVENT_ID)).WillOnce(Return(event_response(200, true)));

	EXPECT_TRUE(service.send_img(large.path()));
}

TEST_F(SpialertServiceTests, SendImgUnsupportedExtensionCreatesNoEvent)
{
	const TempFile gif("capture.gif", "GIF89a");

	EXPECT_FALSE(_service->send_img(gif.path()));
}

TEST_F(SpialertServiceTests, SendImgContentNotMatchingExtensionCreatesNoEvent)
{
	const TempFile jpeg_named_png("capture.png", "\xff\xd8\xff");

	EXPECT_FALSE(_service->send_img(jpeg_named_png.path()));
}

TEST_F(SpialertServiceTests, EveryEventGetsItsOwnUidAndACodeInRange)
{
	std::vector<api::dto::EventRequestDto> sent;
	EXPECT_CALL(*_spialert_api, post_events(_))
		.Times(SEND_COUNT)
		.WillRepeatedly(testing::DoAll(testing::Invoke([&](const auto& event) { sent.push_back(event); }),
		                               Return(event_response(201))));
	EXPECT_CALL(*_spialert_api, post_image(_, _)).Times(SEND_COUNT).WillRepeatedly(Return(event_response(200, true)));

	for (int i = 0; i < SEND_COUNT; ++i)
		_service->send_img(_image.path());

	std::set<std::string> uids;
	for (const auto& event : sent)
	{
		uids.insert(event.event_uid);
		EXPECT_GE(event.event_code, 1);
		EXPECT_LE(event.event_code, 10);
	}
	EXPECT_EQ(uids.size(), static_cast<size_t>(SEND_COUNT));
}
