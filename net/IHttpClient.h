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

enum class FailureEnum : uint8_t
{
	NO_FAILURE,
	TRANSIENT_FAILURE, // NOTE: a resend may succeed, e.g. the server is restarting
	PERMANENT_FAILURE  // NOTE: a resend fails the same way, e.g. the certificate is rejected
};

struct HttpResponse
{
	uint16_t status_code{};
	std::string body;
	std::optional<std::chrono::seconds> retry_after;
	uint32_t error_code{};
	std::string message; // NOTE: the reason phrase with a response, the WinHTTP error text without one
	FailureEnum failure{};

	[[nodiscard]] bool has_response() const noexcept
	{
		return failure == FailureEnum::NO_FAILURE;
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
