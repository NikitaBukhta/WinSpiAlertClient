#ifndef WINSPIALERTCLIENT_SPIALERTAPI_H
#define WINSPIALERTCLIENT_SPIALERTAPI_H

#include <memory>
#include <string>

#include "ISpialertApi.h"
#include "net/IHttpClient.h"

namespace api
{

class SpialertApi : public ISpialertApi
{
public:
	// base_path: path part of base_url, e.g. "/api/v1". http_client must be opened by the caller
	SpialertApi(std::unique_ptr<net::IHttpClient> http_client, std::string base_path, const std::string& api_key);
	~SpialertApi() override = default;

	ApiResponse<dto::StatusDto> get_status() override;

private:
	std::unique_ptr<net::IHttpClient> _http_client;
	std::string _base_path;
	std::string _auth_headers;
};

} // namespace api
#endif // WINSPIALERTCLIENT_SPIALERTAPI_H
