#include "SpialertService.h"

#include <chrono>
#include <format>

#include "api/dto/EventObjectDto.h"
#include "api/dto/EventRequestDto.h"
#include "api/dto/StatusDto.h"
#include "model/Image.h"
#include "model/ImageSizePredictor.h"
#include "utils/FileUtils.h"
#include "utils/UuidGenerator.h"

#define MIN_EVENT_CODE 1
#define MAX_EVENT_CODE 10
// TODO: move to secret/config.json, pass to SpialertService
#define MAX_SEND_ATTEMPTS 3
#define COMPRESSION_HEADROOM (20 * 1024 * 1024)
#define MAX_IMAGE_READ_SIZE (MAX_IMAGE_SIZE + COMPRESSION_HEADROOM)
#define HTTP_CONFLICT 409

namespace app
{

SpialertService::SpialertService(std::unique_ptr<api::ISpialertApi> spialert_api,
                                 std::unique_ptr<model::IImageCompressorFactory> compressor_factory)
	: _spialert_api(std::move(spialert_api)), _compressor_factory(std::move(compressor_factory)),
	  _random(std::random_device{}())
{
}

std::expected<SpialertService::ServerStateEnum, std::string> SpialertService::is_online() const
{
	const auto response = _spialert_api->get_status();
	if (!response.is_success())
		return std::unexpected(std::format("Server status is unknown: {}", describe(response)));

	return response.dto.status ? ServerStateEnum::ONLINE_STATE : ServerStateEnum::OFFLINE_STATE;
}

std::expected<void, std::string> SpialertService::send_img(const std::filesystem::path& image_path)
{
	// NOTE: the image is checked first, or the server keeps an event without an image
	const auto image = load_image(image_path);
	if (!image)
		return std::unexpected(image.error());

	const auto event = create_event(image->file_name());
	if (!event)
		return std::unexpected(event.error());

	return upload_image(*image, event->event_id);
}

std::expected<model::Image, std::string> SpialertService::load_image(const std::filesystem::path& image_path) const
{
	auto image = model::Image::load(image_path, MAX_IMAGE_READ_SIZE);
	if (!image)
		return std::unexpected(std::format("Cannot read {} or it is larger than {} bytes",
		                                   utils::FileUtils::to_utf8(image_path), MAX_IMAGE_READ_SIZE));

	const auto validation_error = image->validation_error();
	if (validation_error)
		return std::unexpected(*validation_error);

	if (image->size() <= MAX_IMAGE_SIZE)
		return std::move(*image);

	return shrink_image(*image);
}

std::expected<model::Image, std::string> SpialertService::shrink_image(const model::Image& image) const
{
	const auto compressor = _compressor_factory->create(image);
	if (!compressor)
		return std::unexpected(std::format("{} is larger than {} bytes and cannot be decoded to shrink it",
		                                   image.file_name(), MAX_IMAGE_SIZE));

	model::ImageSizePredictor predictor(image, MAX_IMAGE_SIZE, compressor->min_side(),
	                                    compressor->decoded_pixel_ratio());
	for (auto step = predictor.next_step(); step; step = predictor.next_step())
	{
		auto compressed = compressor->compress(step->scale, step->quality);
		if (!compressed)
			return std::unexpected(std::format("Cannot re-encode {} to shrink it", image.file_name()));
		if (compressed->size() <= MAX_IMAGE_SIZE)
			return std::move(*compressed);

		predictor.add_measurement(compressed->size());
	}

	return std::unexpected(
		std::format("{} does not fit into {} bytes even at the smallest size", image.file_name(), MAX_IMAGE_SIZE));
}

std::expected<api::dto::EventObjectDto, std::string> SpialertService::create_event(const std::string& file_name)
{
	api::dto::EventRequestDto event;
	event.event_uid = utils::UuidGenerator::generate();
	if (event.event_uid.empty())
		return std::unexpected("Cannot generate event_uid");

	std::uniform_int_distribution<int> event_code(MIN_EVENT_CODE, MAX_EVENT_CODE);
	event.event_code = event_code(_random);
	event.file_name = file_name;
	event.timestamp = std::chrono::system_clock::now();

	// NOTE: a resend keeps event_uid and timestamp, so the server returns the existing event instead of a duplicate
	const auto created = send_with_retry([&] { return _spialert_api->post_events(event); }).response;
	if (!created.is_success())
		return std::unexpected(std::format("Event was not created: {}", describe(created)));

	return created.dto;
}

std::expected<void, std::string> SpialertService::upload_image(const model::Image& image, int event_id) const
{
	const auto [response, resent] = send_with_retry([&] { return _spialert_api->post_image(image, event_id); });

	// NOTE: 409 after a resend means the lost attempt has already uploaded the image
	if (resent && response.status_code == HTTP_CONFLICT)
		return {};

	if (!response.is_success())
		return std::unexpected(std::format("Image for event {} was not uploaded: {}", event_id, describe(response)));

	if (!response.dto.image_uploaded)
		return std::unexpected(
			std::format("Server accepted the image for event {}, but did not mark it as uploaded", event_id));

	return {};
}

SpialertService::SendResult SpialertService::send_with_retry(
	const std::function<api::ApiResponse<api::dto::EventObjectDto>()>& request)
{
	// TODO: backoff between attempts, wait Retry-After on 429, no resend on permanent errors (TLS 12175, DNS 12007)
	// TODO: the delays and the Retry-After cap from secret/config.json
	SendResult result;
	for (int attempt = 0; attempt < MAX_SEND_ATTEMPTS; ++attempt)
	{
		result.resent = attempt > 0;
		result.response = request();
		const auto& error = result.response.error;
		const bool network_error = error && error->source == api::ApiError::SourceEnum::NETWORK_SOURCE;
		if (!network_error)
			break;
	}
	return result;
}

template <typename DtoType>
std::string SpialertService::describe(const api::ApiResponse<DtoType>& response)
{
	if (!response.error)
		return std::format("HTTP {}", response.status_code);
	return response.error->describe();
}

} // namespace app
