#ifndef WINSPIALERTCLIENT_IHTTPCLIENT_H
#define WINSPIALERTCLIENT_IHTTPCLIENT_H

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace net
{

struct HttpResponse
{
	uint16_t status_code{};
	std::string body;
	std::optional<std::chrono::seconds> retry_after;
	uint32_t error_code{};
	std::string message; // NOTE: the reason phrase with a response, the WinHTTP error text without one

	[[nodiscard]] bool has_response() const noexcept
	{
		return error_code == 0;
	}
};

class IHttpClient
{
public:
	virtual ~IHttpClient() = default;
	virtual HttpResponse send_request(const std::string& path, const std::string& method,
	                                  std::span<const std::string_view> body_parts, const std::string& headers) = 0;
};

} // namespace net

#endif // WINSPIALERTCLIENT_IHTTPCLIENT_H
