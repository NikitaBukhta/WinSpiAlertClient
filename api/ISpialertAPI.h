#ifndef WINSPIALERTCLIENT_ISPIALERTAPI_H
#define WINSPIALERTCLIENT_ISPIALERTAPI_H

namespace api
{
namespace dto
{
struct StatusDto;
}

template <typename DtoType>
struct ApiResponse
{
	DtoType dto;
	int status_code;

	[[nodiscard]] bool is_success() const noexcept
	{
		return status_code >= 200 && status_code < 300;
	}
};

class ISpialertApi
{
public:
	virtual ~ISpialertApi() = default;

	virtual ApiResponse<dto::StatusDto> get_status() = 0;
};

} // namespace api

#endif // WINSPIALERTCLIENT_ISPIALERTAPI_H
