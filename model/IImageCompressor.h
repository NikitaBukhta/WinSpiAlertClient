#ifndef WINSPIALERTCLIENT_IIMAGECOMPRESSOR_H
#define WINSPIALERTCLIENT_IIMAGECOMPRESSOR_H

#include <cstdint>
#include <optional>

#include "Image.h"

namespace model
{

class IImageCompressor
{
public:
	virtual ~IImageCompressor() = default;

	[[nodiscard]] virtual uint32_t min_side() const = 0;
	// NOTE: below 1 when a large image was shrunk during decoding.
	[[nodiscard]] virtual double decoded_pixel_ratio() const = 0;
	[[nodiscard]] virtual std::optional<Image> compress(double scale, float quality) const = 0;
};

} // namespace model

#endif // WINSPIALERTCLIENT_IIMAGECOMPRESSOR_H
