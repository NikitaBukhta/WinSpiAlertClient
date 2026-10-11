#ifndef WINSPIALERTCLIENT_APIRESPONSE_H
#define WINSPIALERTCLIENT_APIRESPONSE_H

namespace api
{

template <typename DtoType>
struct ApiResponse
{
	DtoType dto;
	int status_code{};

	[[nodiscard]] bool is_success() const noexcept
	{
		return status_code >= 200 && status_code < 300;
	}
};

} // namespace api

#endif // WINSPIALERTCLIENT_APIRESPONSE_H
