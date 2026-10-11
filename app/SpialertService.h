#ifndef WINSPIALERTCLIENT_SPIALERTSERVICE_H
#define WINSPIALERTCLIENT_SPIALERTSERVICE_H

#include <chrono>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include "IRetryWaiter.h"
#include "api/ISpialertApi.h"
#include "api/dto/EventObjectDto.h"
#include "api/dto/EventsListDto.h"
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

	struct EventsListing
	{
		std::vector<api::dto::EventObjectDto> items;
		int total{};
		bool truncated{};
	};

	SpialertService(std::unique_ptr<api::ISpialertApi> spialert_api,
	                std::unique_ptr<model::IImageCompressorFactory> compressor_factory,
	                std::unique_ptr<IRetryWaiter> retry_waiter);

	[[nodiscard]] std::expected<ServerStateEnum, std::string> is_online() const;
	[[nodiscard]] std::expected<ServerStateEnum, std::string> wait_until_online() const;
	std::expected<void, std::string> send_img(const std::filesystem::path& image_path);
	// uploads into an existing event, e.g. after send created it but the upload failed
	[[nodiscard]] std::expected<void, std::string> upload_img(int event_id,
	                                                          const std::filesystem::path& image_path) const;
	[[nodiscard]] std::expected<EventsListing, std::string> list_events(bool pending) const;

private:
	template <typename ResponseType>
	struct SendResult
	{
		ResponseType response;
		bool resent{};
	};

	[[nodiscard]] std::expected<model::Image, std::string> load_image(const std::filesystem::path& image_path) const;
	[[nodiscard]] std::expected<model::Image, std::string> shrink_image(const model::Image& image) const;
	std::expected<api::dto::EventObjectDto, std::string> create_event(const std::string& file_name);
	[[nodiscard]] std::expected<void, std::string> upload_image(const model::Image& image, int event_id) const;
	[[nodiscard]] std::expected<api::dto::EventsListDto, std::string> request_events_page(
		int offset, std::optional<bool> image_uploaded) const;
	static std::vector<api::dto::EventObjectDto> drop_repeated_events(std::vector<api::dto::EventObjectDto> events);

	template <typename ResponseType>
	static std::string describe(const SendResult<ResponseType>& result);

	template <typename RequestType>
	SendResult<std::invoke_result_t<const RequestType&>> send_with_retry(const RequestType& request) const;
	static bool is_rate_limited(const api::ApiError& error);
	static std::chrono::seconds network_backoff(int attempt);

private:
	std::unique_ptr<api::ISpialertApi> _spialert_api;
	std::unique_ptr<model::IImageCompressorFactory> _compressor_factory;
	std::unique_ptr<IRetryWaiter> _retry_waiter;
	std::mt19937 _random;
};

} // namespace app

#endif // WINSPIALERTCLIENT_SPIALERTSERVICE_H
