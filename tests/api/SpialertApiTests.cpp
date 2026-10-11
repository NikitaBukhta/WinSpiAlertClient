#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <windows.h>
#include <winhttp.h>

#include "api/SpialertApi.h"
#include "api/dto/EventObjectDto.h"
#include "api/dto/EventRequestDto.h"
#include "api/dto/EventsListDto.h"
#include "api/dto/StatusDto.h"
#include "model/Image.h"
#include "net/IHttpClient.h"
#include "net/MultipartBuilder.h"
#include "tests/helpers/RequestBody.h"

#define AUTH_HEADERS "X-API-Key: test-key"
#define JSON_HEADERS AUTH_HEADERS "\r\nContent-Type: application/json"
#define MULTIPART_HEADERS_PREFIX AUTH_HEADERS "\r\nContent-Type: multipart/form-data; boundary="
#define PNG_IMAGE "\x89PNG\r\n\x1a\nimage"
#define EVENT_JSON R"({"event_id":42,"event_uid":"uid-0001","event_code":3,"file_name":"a.png","image_uploaded":false})"

using namespace std::chrono;
using namespace std::string_literals;
using test_helpers::RequestBody;
using testing::_;
using testing::DoAll;
using testing::HasSubstr;
using testing::Return;
using testing::SaveArg;

namespace
{

class MockHttpClient : public net::IHttpClient
{
public:
	MOCK_METHOD(net::HttpResponse, send_request,
	            (const std::string& path, const std::string& method, std::span<const std::string_view> body_parts,
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

api::dto::EventRequestDto make_event()
{
	api::dto::EventRequestDto event;
	event.event_uid = "uid-0001";
	event.event_code = 3;
	event.file_name = "a.png";
	event.timestamp = sys_days{2026y / October / 11} + 12h + 30min + 45s;
	return event;
}

auto body_is(const std::string& expected)
{
	return testing::ResultOf(&RequestBody::join, expected);
}

auto save_body(std::string* body)
{
	return testing::WithArg<2>([body](std::span<const std::string_view> parts) { *body = RequestBody::join(parts); });
}

net::HttpResponse network_error(unsigned long error_code)
{
	net::HttpResponse response;
	response.error_code = error_code;
	response.error_message = "network error";
	return response;
}

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

} // namespace

TEST_F(SpialertApiTests, GetStatusSendsGetWithApiKey)
{
	EXPECT_CALL(*_http_client, send_request("/api/v1/status", "GET", body_is(""), "X-API-Key: test-key"))
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

TEST_F(SpialertApiTests, PostEventsSendsEventAsJson)
{
	const auto event = make_event();
	EXPECT_CALL(*_http_client, send_request("/api/v1/events", "POST",
	                                        body_is(api::dto::EventRequestMapper::toJson(event)), JSON_HEADERS))
		.WillOnce(Return(http_response(201, EVENT_JSON)));

	_api->post_events(event);
}

TEST_F(SpialertApiTests, PostEventsParsesCreatedEvent)
{
	EXPECT_CALL(*_http_client, send_request(_, _, _, _)).WillOnce(Return(http_response(201, EVENT_JSON)));

	const auto response = _api->post_events(make_event());

	EXPECT_TRUE(response.is_success());
	EXPECT_EQ(response.status_code, 201);
	EXPECT_EQ(response.dto.event_id, 42);
	EXPECT_EQ(response.dto.event_uid, "uid-0001");
}

TEST_F(SpialertApiTests, PostEventsRepeatedEventIsSuccess)
{
	EXPECT_CALL(*_http_client, send_request(_, _, _, _)).WillOnce(Return(http_response(200, EVENT_JSON)));

	const auto response = _api->post_events(make_event());

	EXPECT_TRUE(response.is_success());
	EXPECT_EQ(response.dto.event_id, 42);
}

TEST_F(SpialertApiTests, PostEventsConflictIsNotSuccess)
{
	EXPECT_CALL(*_http_client, send_request(_, _, _, _))
		.WillOnce(Return(http_response(409, R"({"error":{"code":"event_uid_conflict","message":"used"}})")));

	const auto response = _api->post_events(make_event());

	EXPECT_FALSE(response.is_success());
	EXPECT_EQ(response.status_code, 409);
	EXPECT_EQ(response.dto.event_id, 0);
}

TEST_F(SpialertApiTests, PostEventsNetworkErrorIsNotSuccess)
{
	EXPECT_CALL(*_http_client, send_request(_, _, _, _)).WillOnce(Return(network_error(ERROR_WINHTTP_TIMEOUT)));

	EXPECT_EQ(_api->post_events(make_event()).status_code, 0);
}

TEST_F(SpialertApiTests, GetEventsSendsPagingQuery)
{
	EXPECT_CALL(*_http_client, send_request("/api/v1/events?limit=20&offset=40", "GET", body_is(""), AUTH_HEADERS))
		.WillOnce(Return(http_response(200, R"({"total":0,"items":[]})")));

	_api->get_events(20, 40, std::nullopt);
}

TEST_F(SpialertApiTests, GetEventsSendsImageUploadedFilter)
{
	EXPECT_CALL(*_http_client, send_request("/api/v1/events?limit=100&offset=0&image_uploaded=false", _, _, _))
		.WillOnce(Return(http_response(200, "{}")));
	EXPECT_CALL(*_http_client, send_request("/api/v1/events?limit=100&offset=0&image_uploaded=true", _, _, _))
		.WillOnce(Return(http_response(200, "{}")));

	_api->get_events(100, 0, false);
	_api->get_events(100, 0, true);
}

TEST_F(SpialertApiTests, GetEventsParsesList)
{
	EXPECT_CALL(*_http_client, send_request(_, _, _, _))
		.WillOnce(Return(http_response(200, R"({"total":1,"limit":20,"offset":0,"items":[)" EVENT_JSON "]}")));

	const auto response = _api->get_events(20, 0, std::nullopt);

	EXPECT_TRUE(response.is_success());
	EXPECT_EQ(response.dto.total, 1);
	ASSERT_EQ(response.dto.items.size(), 1u);
	EXPECT_EQ(response.dto.items[0].event_id, 42);
}

TEST_F(SpialertApiTests, PostImageSendsImageAsMultipart)
{
	const std::string png = "\x89PNG\r\n\x1a\n\0\xff"s;
	std::string body;
	std::string headers;
	EXPECT_CALL(*_http_client, send_request("/api/v1/events/42/image", "POST", _, _))
		.WillOnce(DoAll(save_body(&body), SaveArg<3>(&headers), Return(http_response(200, EVENT_JSON))));

	_api->post_image(model::Image("a.png", png), 42);

	ASSERT_EQ(headers.rfind(MULTIPART_HEADERS_PREFIX, 0), 0u) << headers;
	const std::string boundary = headers.substr(std::string(MULTIPART_HEADERS_PREFIX).size());
	EXPECT_FALSE(boundary.empty());
	EXPECT_EQ(body, net::MultipartBuilder::file_part_header(boundary, "file", "a.png", "image/png") + png +
	                    net::MultipartBuilder::closing_boundary(boundary));
}

TEST_F(SpialertApiTests, PostImageUsesJpegContentTypeForJpg)
{
	std::string body;
	EXPECT_CALL(*_http_client, send_request(_, _, _, _))
		.WillOnce(DoAll(save_body(&body), Return(http_response(200, EVENT_JSON))));

	_api->post_image(model::Image("b.jpg", "\xff\xd8\xff"), 1);

	EXPECT_THAT(body, HasSubstr("filename=\"b.jpg\"\r\nContent-Type: image/jpeg\r\n"));
}

TEST_F(SpialertApiTests, PostImageParsesUpdatedEvent)
{
	EXPECT_CALL(*_http_client, send_request(_, _, _, _))
		.WillOnce(Return(http_response(200, R"({"event_id":42,"image_uploaded":true,"image_size":4})")));

	const auto response = _api->post_image(model::Image("a.png", PNG_IMAGE), 42);

	EXPECT_TRUE(response.is_success());
	EXPECT_TRUE(response.dto.image_uploaded);
	EXPECT_EQ(response.dto.image_size, 4u);
}

TEST_F(SpialertApiTests, PostImageTooLargeForServerIsNotSuccess)
{
	EXPECT_CALL(*_http_client, send_request(_, _, _, _))
		.WillOnce(Return(http_response(413, R"({"error":{"code":"file_too_large","message":"too large"}})")));

	const auto response = _api->post_image(model::Image("a.png", PNG_IMAGE), 42);

	EXPECT_FALSE(response.is_success());
	EXPECT_EQ(response.status_code, 413);
	EXPECT_FALSE(response.dto.image_uploaded);
}

TEST_F(SpialertApiTests, PostImageUnsupportedImageSendsNothing)
{
	EXPECT_EQ(_api->post_image(model::Image("a.gif", "GIF89a"), 42).status_code, 0);
	EXPECT_EQ(_api->post_image(model::Image("a.png", "\xff\xd8\xff"), 42).status_code, 0);
}

TEST_F(SpialertApiTests, PostImageSendsImageBytesWithoutCopy)
{
	const model::Image image("a.png", PNG_IMAGE);
	bool image_bytes_passed_as_is = false;
	EXPECT_CALL(*_http_client, send_request(_, _, _, _))
		.WillOnce(DoAll(testing::WithArg<2>([&](std::span<const std::string_view> parts) {
							for (const std::string_view part : parts)
								image_bytes_passed_as_is |= part.data() == image.data().data();
						}),
		                Return(http_response(200, EVENT_JSON))));

	_api->post_image(image, 42);

	EXPECT_TRUE(image_bytes_passed_as_is) << "the image must be one of the body parts, not copied into a new body";
}
