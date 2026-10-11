#include "SpialertApi.h"

#include <format>
#include <print>

#include "dto/EventObjectDto.h"
#include "dto/EventRequestDto.h"
#include "dto/EventsListDto.h"
#include "dto/StatusDto.h"
#include "model/Image.h"
#include "net/MultipartBuilder.h"
#include "utils/UuidGenerator.h"

#define API_KEY_HEADER "X-API-Key: "
#define CONTENT_TYPE_HEADER "\r\nContent-Type: "
#define JSON_CONTENT_TYPE "application/json"
#define STATUS_PATH "/status"
#define EVENTS_PATH "/events"
#define IMAGE_PATH_FORMAT "/events/{}/image"
#define EVENTS_QUERY_FORMAT "?limit={}&offset={}"
#define IMAGE_UPLOADED_QUERY_FORMAT "&image_uploaded={}"
#define IMAGE_FIELD_NAME "file"

namespace api
{
SpialertApi::SpialertApi(std::unique_ptr<net::IHttpClient> http_client, std::string base_path,
                         const std::string& api_key)
	: _http_client(std::move(http_client)), _base_path(std::move(base_path)), _auth_headers(API_KEY_HEADER + api_key)
{
}

ApiResponse<dto::StatusDto> SpialertApi::get_status()
{
	return execute_request("GET", STATUS_PATH, {}, {}, dto::StatusMapper::fromJson);
}

ApiResponse<dto::EventObjectDto> SpialertApi::post_events(const dto::EventRequestDto& event)
{
	const std::string json = dto::EventRequestMapper::toJson(event);
	const std::string_view body_parts[] = {json};

	return execute_request("POST", EVENTS_PATH, body_parts, JSON_CONTENT_TYPE, dto::EventObjectMapper::fromJson);
}

ApiResponse<dto::EventsListDto> SpialertApi::get_events(int limit, int offset, std::optional<bool> image_uploaded)
{
	std::string path = EVENTS_PATH + std::format(EVENTS_QUERY_FORMAT, limit, offset);
	if (image_uploaded)
		path += std::format(IMAGE_UPLOADED_QUERY_FORMAT, *image_uploaded);

	return execute_request("GET", path, {}, {}, dto::EventsListMapper::fromJson);
}

ApiResponse<dto::EventObjectDto> SpialertApi::post_image(const model::Image& image, int event_id)
{
	const auto content_type = image.content_type();
	if (!content_type)
		return {};

	const std::string boundary = utils::UuidGenerator::generate();
	if (boundary.empty())
	{
		std::println(stderr, "Cannot create a multipart boundary");
		return {};
	}

	const std::string header =
		net::MultipartBuilder::file_part_header(boundary, IMAGE_FIELD_NAME, image.file_name(), *content_type);
	const std::string closing = net::MultipartBuilder::closing_boundary(boundary);
	const std::string_view body_parts[] = {header, image.data(), closing};

	return execute_request("POST", std::format(IMAGE_PATH_FORMAT, event_id), body_parts,
	                       net::MultipartBuilder::content_type(boundary), dto::EventObjectMapper::fromJson);
}

template <typename DtoType>
ApiResponse<DtoType> SpialertApi::execute_request(const std::string& method, const std::string& path,
                                                  std::span<const std::string_view> body_parts,
                                                  const std::string& content_type, DtoType (*from_json)(char*))
{
	// TODO: return the error kind (WinHTTP code, parse error) + message to the caller instead of printing here:
	// SpialertService decides on resends (send_with_resend) and needs it to tell a timeout from a TLS failure
	const std::string headers =
		content_type.empty() ? _auth_headers : _auth_headers + CONTENT_TYPE_HEADER + content_type;
	net::HttpResponse response = _http_client->send_request(_base_path + path, method, body_parts, headers);

	if (!response.has_response())
	{
		std::println(stderr, "{} {} failed: {}", method, path, response.error_message);
		return {};
	}

	ApiResponse<DtoType> result{};
	result.status_code = static_cast<int>(response.status_code);

	if (result.is_success())
		result.dto = from_json(response.body.data());

	return result;
}
} // namespace api
