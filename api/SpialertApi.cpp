#include "SpialertApi.h"

#include <format>

#include "ApiConstants.h"
#include "dto/EventObjectDto.h"
#include "dto/EventRequestDto.h"
#include "dto/EventsListDto.h"
#include "dto/ServerErrorDto.h"
#include "dto/StatusDto.h"
#include "model/Image.h"
#include "net/MultipartBuilder.h"
#include "utils/UuidGenerator.h"

#define CONTENT_TYPE_HEADER "\r\nContent-Type: "
#define JSON_CONTENT_TYPE "application/json"
#define IMAGE_PATH_FORMAT "/events/{}/image"
#define EVENTS_QUERY_FORMAT "?limit={}&offset={}"
#define IMAGE_UPLOADED_QUERY_FORMAT "&image_uploaded={}"
#define IMAGE_FIELD_NAME "file"
#define NO_BOUNDARY_ERROR "Cannot create a multipart boundary"

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

	return require_event_id(
		execute_request("POST", EVENTS_PATH, body_parts, JSON_CONTENT_TYPE, dto::EventObjectMapper::fromJson));
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
		return local_error<dto::EventObjectDto>(image.validation_error().value_or(""));

	const std::string boundary = utils::UuidGenerator::generate();
	if (boundary.empty())
		return local_error<dto::EventObjectDto>(NO_BOUNDARY_ERROR);

	const std::string header =
		net::MultipartBuilder::file_part_header(boundary, IMAGE_FIELD_NAME, image.file_name(), *content_type);
	const std::string closing = net::MultipartBuilder::closing_boundary(boundary);
	const std::string_view body_parts[] = {header, image.data(), closing};

	return require_event_id(execute_request("POST", std::format(IMAGE_PATH_FORMAT, event_id), body_parts,
	                                        net::MultipartBuilder::content_type(boundary),
	                                        dto::EventObjectMapper::fromJson));
}

template <typename DtoType>
ApiResponse<DtoType> SpialertApi::execute_request(const std::string& method, const std::string& path,
                                                  std::span<const std::string_view> body_parts,
                                                  const std::string& content_type,
                                                  std::optional<DtoType> (*from_json)(char*))
{
	const std::string headers =
		content_type.empty() ? _auth_headers : _auth_headers + CONTENT_TYPE_HEADER + content_type;
	net::HttpResponse response = _http_client->send_request(_base_path + path, method, body_parts, headers);

	ApiResponse<DtoType> result{};
	if (!response.has_response())
	{
		result.error = ApiError::network(response.error_code, std::move(response.message), response.failure);
		return result;
	}

	result.status_code = response.status_code;
	result.retry_after = response.retry_after;
	if (!result.is_success())
	{
		const auto server_error = dto::ServerErrorMapper::fromJson(response.body.data());
		result.error = ApiError::server(result.status_code, std::move(response.message), server_error.value_or({}));
		return result;
	}

	auto dto = from_json(response.body.data());
	if (!dto)
	{
		result.error = ApiError::unreadable_response(result.status_code);
		return result;
	}

	result.dto = std::move(*dto);
	return result;
}

template <typename DtoType>
ApiResponse<DtoType> SpialertApi::local_error(std::string message)
{
	ApiResponse<DtoType> result{};
	result.error = ApiError::local(std::move(message));
	return result;
}
ApiResponse<dto::EventObjectDto> SpialertApi::require_event_id(ApiResponse<dto::EventObjectDto> response)
{
	const bool event_without_id = response.is_success() && response.dto.event_id <= 0;
	if (event_without_id)
		response.error = ApiError::unreadable_response(response.status_code);
	return response;
}

} // namespace api
