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
#include "model/IImageCompressorFactory.h"

namespace app
{

class SpialertService
{
public:
	SpialertService(std::unique_ptr<api::ISpialertApi> spialert_api,
	                std::unique_ptr<model::IImageCompressorFactory> compressor_factory);

	[[nodiscard]] bool is_online() const;
	bool send_img(const std::filesystem::path& image_path);

private:
	struct SendResult
	{
		api::ApiResponse<api::dto::EventObjectDto> response;
		bool resent{};
	};

	[[nodiscard]] std::optional<model::Image> load_image(const std::filesystem::path& image_path) const;
	[[nodiscard]] std::optional<model::Image> shrink_image(const model::Image& image) const;
	std::optional<api::dto::EventObjectDto> create_event(const std::string& file_name);
	[[nodiscard]] bool upload_image(const model::Image& image, int event_id) const;
	static SendResult send_with_retry(const std::function<api::ApiResponse<api::dto::EventObjectDto>()>& request);

private:
	std::unique_ptr<api::ISpialertApi> _spialert_api;
	std::unique_ptr<model::IImageCompressorFactory> _compressor_factory;
	std::mt19937 _random;
};

} // namespace app

#endif // WINSPIALERTCLIENT_SPIALERTSERVICE_H
