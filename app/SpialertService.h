#ifndef WINSPIALERTCLIENT_SPIALERTSERVICE_H
#define WINSPIALERTCLIENT_SPIALERTSERVICE_H

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
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
	enum class ServerStateEnum : uint8_t
	{
		ONLINE_STATE,
		OFFLINE_STATE
	};

	SpialertService(std::unique_ptr<api::ISpialertApi> spialert_api,
	                std::unique_ptr<model::IImageCompressorFactory> compressor_factory);

	[[nodiscard]] std::expected<ServerStateEnum, std::string> is_online() const;
	std::expected<void, std::string> send_img(const std::filesystem::path& image_path);

private:
	struct SendResult
	{
		api::ApiResponse<api::dto::EventObjectDto> response;
		bool resent{};
	};

	[[nodiscard]] std::expected<model::Image, std::string> load_image(const std::filesystem::path& image_path) const;
	[[nodiscard]] std::expected<model::Image, std::string> shrink_image(const model::Image& image) const;
	std::expected<api::dto::EventObjectDto, std::string> create_event(const std::string& file_name);
	[[nodiscard]] std::expected<void, std::string> upload_image(const model::Image& image, int event_id) const;

	template <typename DtoType>
	static std::string describe(const api::ApiResponse<DtoType>& response);

	static SendResult send_with_retry(const std::function<api::ApiResponse<api::dto::EventObjectDto>()>& request);

private:
	std::unique_ptr<api::ISpialertApi> _spialert_api;
	std::unique_ptr<model::IImageCompressorFactory> _compressor_factory;
	std::mt19937 _random;
};

} // namespace app

#endif // WINSPIALERTCLIENT_SPIALERTSERVICE_H
