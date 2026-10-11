#ifndef WINSPIALERTCLIENT_IMAGESIZEPREDICTOR_H
#define WINSPIALERTCLIENT_IMAGESIZEPREDICTOR_H

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Image.h"

namespace model
{

class ImageSizePredictor
{
public:
	struct Step
	{
		double scale{};
		float quality{}; // JPEG only
	};

	ImageSizePredictor(const Image& image, size_t max_size, uint32_t min_side, double decoded_pixel_ratio);

	std::optional<Step> next_step();
	void add_measurement(size_t size);

private:
	[[nodiscard]] Step predict_step();

private:
	double _max_size;
	double _min_scale;
	bool _quality_first;
	bool _done{};
	int _steps{};
	Step _last_step;
	double _last_size;
};

} // namespace model

#endif // WINSPIALERTCLIENT_IMAGESIZEPREDICTOR_H
