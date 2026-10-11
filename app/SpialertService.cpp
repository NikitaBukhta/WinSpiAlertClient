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
// TODO: move to secret/config.json, pass to SpialertService
#define NETWORK_BACKOFF_BASE std::chrono::seconds{1}
// TODO: move to secret/config.json, pass to SpialertService
#define DEFAULT_RETRY_AFTER std::chrono::seconds{5}
// NOTE: the server limit is 60 requests per minute, a longer wait means something else is wrong
// TODO: move to secret/config.json, pass to SpialertService
#define MAX_RETRY_AFTER std::chrono::seconds{60}
#define COMPRESSION_HEADROOM (20 * 1024 * 1024)
#define MAX_IMAGE_READ_SIZE (MAX_IMAGE_SIZE + COMPRESSION_HEADROOM)
#define HTTP_CONFLICT 409
#define HTTP_TOO_MANY_REQUESTS 429

namespace app
{

SpialertService::SpialertService(std::unique_ptr<api::ISpialertApi> spialert_api,
                                 std::unique_ptr<model::IImageCompressorFactory> compressor_factory,
                                 std::unique_ptr<IRetryWaiter> retry_waiter)
	: _spialert_api(std::move(spialert_api)), _compressor_factory(std::move(compressor_factory)),
	  _retry_waiter(std::move(retry_waiter)), _random(std::random_device{}())
{
}

std::expected<SpialertService::ServerStateEnum, std::string> SpialertService::is_online() const
{
	const auto status = send_with_retry([&] { return _spialert_api->get_status(); });
	if (!status.response.is_success())
		return std::unexpected(std::format("Server status is unknown: {}", describe(status)));

	return status.response.dto.status ? ServerStateEnum::ONLINE_STATE : ServerStateEnum::OFFLINE_STATE;
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
	const auto created = send_with_retry([&] { return _spialert_api->post_events(event); });
	if (!created.response.is_success())
		return std::unexpected(std::format("Event was not created: {}", describe(created)));

	return created.response.dto;
}

std::expected<void, std::string> SpialertService::upload_image(const model::Image& image, int event_id) const
{
	const auto uploaded = send_with_retry([&] { return _spialert_api->post_image(image, event_id); });
	const auto& response = uploaded.response;

	// NOTE: 409 after a resend means the lost attempt has already uploaded the image
	if (uploaded.resent && response.status_code == HTTP_CONFLICT)
		return {};

	if (!response.is_success())
		return std::unexpected(std::format("Image for event {} was not uploaded: {}", event_id, describe(uploaded)));

	if (!response.dto.image_uploaded)
		return std::unexpected(
			std::format("Server accepted the image for event {}, but did not mark it as uploaded", event_id));

	return {};
}

template <typename RequestType>
SpialertService::SendResult<std::invoke_result_t<const RequestType&>> SpialertService::send_with_retry(
	const RequestType& request) const
{
	SendResult<std::invoke_result_t<const RequestType&>> result;
	for (int attempt = 1; attempt <= MAX_SEND_ATTEMPTS; ++attempt)
	{
		result.response = request();
		const auto& error = result.response.error;
		const bool rate_limited = error && is_rate_limited(*error);
		const bool network_error = error && error->is_transient_network_error();
		if (!rate_limited && !network_error)
			break;

		const auto delay =
			rate_limited ? result.response.retry_after.value_or(DEFAULT_RETRY_AFTER) : network_backoff(attempt);
		const bool wait_too_long = rate_limited && delay > MAX_RETRY_AFTER;
		if (wait_too_long)
		{
			result.give_up_reason =
				std::format("server asks to wait {} s, more than {} s", delay.count(), MAX_RETRY_AFTER.count());
			break;
		}
		if (attempt == MAX_SEND_ATTEMPTS)
			break;

		// NOTE: 429 means the server has not processed the request, so only a network error makes the outcome unknown
		result.resent = result.resent || network_error;
		_retry_waiter->wait(delay, rate_limited ? IRetryWaiter::RetryReasonEnum::RATE_LIMIT_REASON
		                                        : IRetryWaiter::RetryReasonEnum::NETWORK_REASON);
	}
	return result;
}

bool SpialertService::is_rate_limited(const api::ApiError& error)
{
	return error.source == api::ApiError::SourceEnum::SERVER_SOURCE && error.status_code == HTTP_TOO_MANY_REQUESTS;
}

std::chrono::seconds SpialertService::network_backoff(const int attempt)
{
	return NETWORK_BACKOFF_BASE * (1 << (attempt - 1));
}

template <typename ResponseType>
std::string SpialertService::describe(const SendResult<ResponseType>& result)
{
	const auto& response = result.response;
	std::string text = response.error ? response.error->describe() : std::format("HTTP {}", response.status_code);
	if (!result.give_up_reason.empty())
		text += std::format(" ({})", result.give_up_reason);
	return text;
}

} // namespace app
