#include "SpialertApi.h"

#include <print>

#include "dto/StatusDto.h"

#define API_KEY_HEADER "X-API-Key: "
#define STATUS_PATH "/status"

namespace api
{
SpialertApi::SpialertApi(std::unique_ptr<net::IHttpClient> http_client, std::string base_path,
                         const std::string& api_key)
	: _http_client(std::move(http_client)), _base_path(std::move(base_path)), _auth_headers(API_KEY_HEADER + api_key)
{
}

ApiResponse<dto::StatusDto> SpialertApi::get_status()
{
	// TODO: retry policy shared by all API calls:
	//  - transient (retry, max N attempts, backoff 1s/2s/4s): WinHTTP 12002/12029/12030/12152, HTTP 5xx
	//  - HTTP 429: wait Retry-After (capped), then retry
	//  - permanent (fail fast): 12175 TLS, 12007 DNS, HTTP 4xx
	//  - return error kind + message to the caller instead of printing here
	net::HttpResponse response = _http_client->send_request(_base_path + STATUS_PATH, "GET", {}, _auth_headers);

	if (!response.has_response())
	{
		std::println(stderr, "GET {} failed: {}", STATUS_PATH, response.error_message);
		return {};
	}

	ApiResponse<dto::StatusDto> result{};
	result.status_code = static_cast<int>(response.status_code);

	if (response.status_code == 200)
		result.dto = dto::StatusMapper::fromJson(response.body.data());

	return result;
}
} // namespace api
