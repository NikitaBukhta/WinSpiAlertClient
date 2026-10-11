#ifndef WINSPIALERTCLIENT_IIMAGECOMPRESSORFACTORY_H
#define WINSPIALERTCLIENT_IIMAGECOMPRESSORFACTORY_H

#include <memory>

#include "IImageCompressor.h"
#include "Image.h"

namespace model
{

class IImageCompressorFactory
{
public:
	virtual ~IImageCompressorFactory() = default;

	[[nodiscard]] virtual std::unique_ptr<IImageCompressor> create(const Image& image) const = 0;
};

} // namespace model

#endif // WINSPIALERTCLIENT_IIMAGECOMPRESSORFACTORY_H
