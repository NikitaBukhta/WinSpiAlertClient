#include "ApiError.h"

#include <format>

#define NETWORK_ERROR_FORMAT "network error: {}"
#define NOT_JSON_ERROR_FORMAT "{} (response is not a JSON error)"
#define UNREADABLE_RESPONSE_FORMAT "HTTP {}, but the response body cannot be read"
#define DETAILS_SEPARATOR "; "

namespace api
{

ApiError ApiError::network(const uint32_t network_code, std::string message, const net::FailureEnum network_failure)
{
	ApiError error;
	error.source = SourceEnum::NETWORK_SOURCE;
	error.network_code = network_code;
	error.network_failure = network_failure;
	error.message = std::move(message);
	return error;
}

ApiError ApiError::server(const int status_code, std::string message, dto::ServerErrorDto server_error)
{
	ApiError error;
	error.source = SourceEnum::SERVER_SOURCE;
	error.status_code = status_code;
	error.message = std::move(message);
	error.server_error = std::move(server_error);
	return error;
}

ApiError ApiError::local(std::string message)
{
	ApiError error;
	error.source = SourceEnum::LOCAL_SOURCE;
	error.message = std::move(message);
	return error;
}

ApiError ApiError::unreadable_response(const int status_code)
{
	ApiError error;
	error.source = SourceEnum::RESPONSE_SOURCE;
	error.status_code = status_code;
	return error;
}

std::string ApiError::describe() const
{
	switch (source)
	{
	case SourceEnum::NETWORK_SOURCE:
		return std::format(NETWORK_ERROR_FORMAT, message);
	case SourceEnum::SERVER_SOURCE:
		return describe_server_error();
	case SourceEnum::RESPONSE_SOURCE:
		return std::format(UNREADABLE_RESPONSE_FORMAT, status_code);
	case SourceEnum::LOCAL_SOURCE:
		break;
	}
	return message;
}

bool ApiError::is_transient_network_error() const
{
	return network_failure == net::FailureEnum::TRANSIENT_FAILURE;
}

std::string ApiError::describe_server_error() const
{
	const bool is_json_error = !server_error.code.empty() || !server_error.message.empty();
	if (!is_json_error)
		return describe_not_json_error();

	std::string text = std::to_string(status_code);
	if (!server_error.code.empty())
		text += " " + server_error.code;

	// NOTE: the message of validation_error only summarizes its details
	const std::string reason = server_error.details.empty() ? server_error.message : join_details();
	if (!reason.empty())
		text += ": " + reason;

	return text;
}

std::string ApiError::describe_not_json_error() const
{
	// NOTE: the body is not shown, it may be an HTML page of a proxy
	const std::string status =
		message.empty() ? std::to_string(status_code) : std::format("{} {}", status_code, message);
	return std::format(NOT_JSON_ERROR_FORMAT, status);
}

std::string ApiError::join_details() const
{
	std::string text;
	for (const auto& detail : server_error.details)
	{
		if (!text.empty())
			text += DETAILS_SEPARATOR;
		text += detail.field.empty() ? detail.message : detail.field + ": " + detail.message;
	}
	return text;
}

} // namespace api
