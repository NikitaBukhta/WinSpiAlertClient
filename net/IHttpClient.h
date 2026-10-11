#ifndef WINSPIALERTCLIENT_IHTTPCLIENT_H
#define WINSPIALERTCLIENT_IHTTPCLIENT_H

#include <chrono>
#include <optional>
#include <string>

namespace net
{

struct HttpResponse
{
	unsigned long status_code{};
	std::string body;
	std::optional<std::chrono::seconds> retry_after;
	unsigned long error_code{};
	std::string error_message;

	[[nodiscard]] bool has_response() const noexcept
	{
		return error_code == 0;
	}
};

class IHttpClient
{
public:
	virtual ~IHttpClient() = default;
	virtual bool open() = 0;
	virtual bool close() = 0;
	virtual HttpResponse send_request(const std::string& path, const std::string& method, const std::string& body,
	                                  const std::string& headers) = 0;
};

} // namespace net

#endif // WINSPIALERTCLIENT_IHTTPCLIENT_H
