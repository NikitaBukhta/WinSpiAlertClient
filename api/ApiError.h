#ifndef WINSPIALERTCLIENT_APIERROR_H
#define WINSPIALERTCLIENT_APIERROR_H

#include <cstdint>
#include <string>

#include "dto/ServerErrorDto.h"

namespace api
{

struct ApiError
{
	enum class SourceEnum : uint8_t
	{
		NETWORK_SOURCE,
		SERVER_SOURCE,
		LOCAL_SOURCE // the request was not sent
	};

	SourceEnum source{};
	int status_code{};
	uint32_t network_code{};
	std::string message; // NOTE: the reason phrase for SERVER_SOURCE, the error text otherwise
	dto::ServerErrorDto server_error;

	static ApiError network(uint32_t network_code, std::string message);
	static ApiError server(int status_code, std::string message, dto::ServerErrorDto server_error);
	static ApiError local(std::string message);

	[[nodiscard]] std::string describe() const;

private:
	[[nodiscard]] std::string describe_server_error() const;
	[[nodiscard]] std::string describe_not_json_error() const;
	[[nodiscard]] std::string join_details() const;
};

} // namespace api

#endif // WINSPIALERTCLIENT_APIERROR_H
