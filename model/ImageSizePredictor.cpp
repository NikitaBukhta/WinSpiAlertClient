#include "ImageSizePredictor.h"

#include <algorithm>

#define MAX_STEPS 4
#define JPEG_QUALITY_ONLY_MAX_RATIO 1.5
#define JPEG_MIN_QUALITY 0.75f
#define JPEG_LAST_QUALITY 0.6f
#define MIN_SIDE_PX 640.0
#define SCALE_MARGIN 0.9

namespace model
{

ImageSizePredictor::ImageSizePredictor(const Image& image, const size_t max_size, const uint32_t min_side,
                                       const double decoded_pixel_ratio)
	: _max_size(static_cast<double>(max_size)), _min_scale(min_side ? std::min(1.0, MIN_SIDE_PX / min_side) : 1.0),
	  _last_step{.scale = 1.0, .quality = JPEG_MIN_QUALITY},
	  _last_size(static_cast<double>(image.size()) * decoded_pixel_ratio)
{
	// NOTE: PNG goes straight to scaling. WIC has no PNG compression level, so re-encoding barely helps.
	const bool is_jpeg = image.format() == Image::FormatEnum::JPEG_FORMAT;
	_quality_first = is_jpeg && _last_size / _max_size <= JPEG_QUALITY_ONLY_MAX_RATIO;
}

std::optional<ImageSizePredictor::Step> ImageSizePredictor::next_step()
{
	if (_done || _steps == MAX_STEPS)
		return std::nullopt;

	++_steps;
	_last_step = predict_step();
	return _last_step;
}

void ImageSizePredictor::add_measurement(size_t size)
{
	_last_size = static_cast<double>(size);
}

ImageSizePredictor::Step ImageSizePredictor::predict_step()
{
	if (_steps == 1 && _quality_first)
		return {1.0, JPEG_MIN_QUALITY};

	// NOTE: the size is roughly proportional to the pixel count
	const double scale = std::min(1.0, _last_step.scale * std::sqrt(_max_size / _last_size) * SCALE_MARGIN);

	if (scale > _min_scale && _steps < MAX_STEPS)
		return {scale, JPEG_MIN_QUALITY};

	_done = true;
	return {.scale = _min_scale, .quality = JPEG_LAST_QUALITY};
}

} // namespace model
