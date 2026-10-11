#include "SpialertService.h"

#include <chrono>
#include <print>

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

bool SpialertService::is_online() const
{
	const auto response = _spialert_api->get_status();
	return response.is_success() && response.dto.status;
}

bool SpialertService::send_img(const std::filesystem::path& image_path)
{
	// the image is checked before the event is created: otherwise the server keeps an event that never gets its image
	const auto image = load_image(image_path);
	if (!image)
		return false;

	const auto event = create_event(image->file_name());
	if (!event)
		return false;

	return upload_image(*image, event->event_id);
}

std::optional<model::Image> SpialertService::load_image(const std::filesystem::path& image_path) const
{
	auto image = model::Image::load(image_path, MAX_IMAGE_READ_SIZE);
	if (!image)
	{
		std::println(stderr, "Cannot read {} or it is larger than {} bytes", utils::FileUtils::to_utf8(image_path),
		             MAX_IMAGE_READ_SIZE);
		return std::nullopt;
	}

	if (const auto validation_error = image->validation_error())
	{
		std::println(stderr, "{}", *validation_error);
		return std::nullopt;
	}

	if (image->size() <= MAX_IMAGE_SIZE)
		return image;

	return shrink_image(*image);
}

std::optional<model::Image> SpialertService::shrink_image(const model::Image& image) const
{
	const auto compressor = _compressor_factory->create(image);
	if (!compressor)
	{
		std::println(stderr, "{} is larger than {} bytes and cannot be decoded to shrink it", image.file_name(),
		             MAX_IMAGE_SIZE);
		return std::nullopt;
	}

	model::ImageSizePredictor predictor(image, MAX_IMAGE_SIZE, compressor->min_side(),
	                                    compressor->decoded_pixel_ratio());
	for (auto step = predictor.next_step(); step; step = predictor.next_step())
	{
		auto compressed = compressor->compress(step->scale, step->quality);
		if (!compressed)
		{
			std::println(stderr, "Cannot re-encode {} to shrink it", image.file_name());
			return std::nullopt;
		}
		if (compressed->size() <= MAX_IMAGE_SIZE)
			return compressed;

		predictor.add_measurement(compressed->size());
	}

	std::println(stderr, "{} does not fit into {} bytes even at the smallest size", image.file_name(), MAX_IMAGE_SIZE);
	return std::nullopt;
}

std::optional<api::dto::EventObjectDto> SpialertService::create_event(const std::string& file_name)
{
	api::dto::EventRequestDto event;
	event.event_uid = utils::UuidGenerator::generate();
	if (event.event_uid.empty())
	{
		std::println(stderr, "Cannot generate event_uid");
		return std::nullopt;
	}

	std::uniform_int_distribution<int> event_code(MIN_EVENT_CODE, MAX_EVENT_CODE);
	event.event_code = event_code(_random);
	event.file_name = file_name;
	event.timestamp = std::chrono::system_clock::now();

	// NOTE: a resend repeats the same event (same event_uid and timestamp): if the lost attempt reached the server,
	// it answers 200 with the already created event instead of creating a duplicate
	const auto created = send_with_retry([&] { return _spialert_api->post_events(event); }).response;
	if (!created.is_success())
	{
		std::println(stderr, "Event was not created (HTTP {})", created.status_code);
		return std::nullopt;
	}

	return created.dto;
}

bool SpialertService::upload_image(const model::Image& image, int event_id) const
{
	const auto [response, resent] = send_with_retry([&] { return _spialert_api->post_image(image, event_id); });

	// NOTE: a lost attempt may have uploaded the image, then the resend gets 409 image_already_uploaded
	if (resent && response.status_code == HTTP_CONFLICT)
		return true;

	if (!response.is_success())
	{
		std::println(stderr, "Image for event {} was not uploaded (HTTP {})", event_id, response.status_code);
		return false;
	}

	if (!response.dto.image_uploaded)
	{
		std::println(stderr, "Server accepted the image for event {}, but did not mark it as uploaded", event_id);
		return false;
	}

	return true;
}

SpialertService::SendResult SpialertService::send_with_retry(
	const std::function<api::ApiResponse<api::dto::EventObjectDto>()>& request)
{
	// TODO: backoff between attempts, wait Retry-After on 429, no resend on permanent errors (TLS 12175, DNS 12007):
	// needs the error kind from SpialertApi, not only status_code == 0
	// TODO: the delays and the Retry-After cap from secret/config.json
	SendResult result;
	for (int attempt = 0; attempt < MAX_SEND_ATTEMPTS; ++attempt)
	{
		result.resent = attempt > 0;
		result.response = request();
		const bool network_error = result.response.status_code == 0;
		if (!network_error)
			break;
	}
	return result;
}

} // namespace app
