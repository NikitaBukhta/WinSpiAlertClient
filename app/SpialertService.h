#ifndef WINSPIALERTCLIENT_SPIALERTSERVICE_H
#define WINSPIALERTCLIENT_SPIALERTSERVICE_H

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>

#include "api/ISpialertApi.h"
#include "api/dto/EventObjectDto.h"

namespace app
{

class SpialertService
{
public:
	explicit SpialertService(std::unique_ptr<api::ISpialertApi> spialert_api);

	bool is_online() const;
	bool send_img(const std::filesystem::path& image_path);

private:
	struct SendResult
	{
		api::ApiResponse<api::dto::EventObjectDto> response;
		bool resent{};
	};

	static std::optional<model::Image> load_image(const std::filesystem::path& image_path);
	std::optional<api::dto::EventObjectDto> create_event(const std::string& file_name);
	bool upload_image(const model::Image& image, int event_id) const;
	static SendResult send_with_retry(const std::function<api::ApiResponse<api::dto::EventObjectDto>()>& request);

private:
	std::unique_ptr<api::ISpialertApi> _spialert_api;
	std::mt19937 _random;
};

} // namespace app

#endif // WINSPIALERTCLIENT_SPIALERTSERVICE_H
