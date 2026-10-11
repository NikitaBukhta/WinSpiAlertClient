#ifndef WINSPIALERTCLIENT_WICIMAGECOMPRESSORFACTORY_H
#define WINSPIALERTCLIENT_WICIMAGECOMPRESSORFACTORY_H

#include "IImageCompressorFactory.h"

namespace model
{

class WicImageCompressorFactory : public IImageCompressorFactory
{
public:
	[[nodiscard]] std::unique_ptr<IImageCompressor> create(const Image& image) const override;
};

} // namespace model

#endif // WINSPIALERTCLIENT_WICIMAGECOMPRESSORFACTORY_H
