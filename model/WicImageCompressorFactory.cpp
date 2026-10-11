#include "WicImageCompressorFactory.h"

#include "WicImageCompressor.h"

namespace model
{

std::unique_ptr<IImageCompressor> WicImageCompressorFactory::create(const Image& image) const
{
	auto compressor = std::make_unique<WicImageCompressor>(image);
	if (!compressor->is_decoded())
		return nullptr;

	return compressor;
}

} // namespace model
