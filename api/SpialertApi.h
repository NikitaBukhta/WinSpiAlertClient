#ifndef WINSPIALERTCLIENT_SPIALERTAPI_H
#define WINSPIALERTCLIENT_SPIALERTAPI_H

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ISpialertApi.h"
#include "net/IHttpClient.h"

namespace api
{

class SpialertApi : public ISpialertApi
{
public:
	// base_path: path part of base_url, e.g. "/api/v1"
	SpialertApi(std::unique_ptr<net::IHttpClient> http_client, std::string base_path, const std::string& api_key);
	~SpialertApi() override = default;

	ApiResponse<dto::StatusDto> get_status() override;
	ApiResponse<dto::EventObjectDto> post_events(const dto::EventRequestDto& event) override;
	ApiResponse<dto::EventsListDto> get_events(int limit, int offset, std::optional<bool> image_uploaded) override;
	ApiResponse<dto::EventObjectDto> post_image(const model::Image& image, int event_id) override;

private:
	template <typename DtoType>
	ApiResponse<DtoType> execute_request(const std::string& method, const std::string& path,
	                                     std::span<const std::string_view> body_parts, const std::string& content_type,
	                                     DtoType (*from_json)(char*));
	template <typename DtoType>
	static ApiResponse<DtoType> local_error(std::string message);

private:
	std::unique_ptr<net::IHttpClient> _http_client;
	std::string _base_path;
	std::string _auth_headers;
};

} // namespace api
#endif // WINSPIALERTCLIENT_SPIALERTAPI_H
