#ifndef WINSPIALERTCLIENT_WICIMAGECOMPRESSOR_H
#define WINSPIALERTCLIENT_WICIMAGECOMPRESSOR_H

#include <cstdint>
#include <optional>
#include <string>

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "IImageCompressor.h"
#include "Image.h"
#include "utils/ComScope.h"

namespace model
{

class WicImageCompressor : public IImageCompressor
{
public:
	explicit WicImageCompressor(const Image& image);

	[[nodiscard]] bool is_decoded() const noexcept;
	[[nodiscard]] uint32_t min_side() const override;
	[[nodiscard]] double decoded_pixel_ratio() const override;
	[[nodiscard]] std::optional<Image> compress(double scale, float quality) const override;

private:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	void decode(const Image& image);
	bool create_factory();
	[[nodiscard]] ComPtr<IWICBitmapFrameDecode> open_frame(const Image& image) const;
	[[nodiscard]] ComPtr<IWICBitmapSource> to_encoder_format(IWICBitmapSource* source) const;

	[[nodiscard]] ComPtr<IWICBitmapSource> transformed(double scale) const;
	[[nodiscard]] ComPtr<IWICBitmapSource> rotated(IWICBitmapSource* source) const;
	[[nodiscard]] ComPtr<IWICBitmapSource> scaled(IWICBitmapSource* source, double scale,
	                                              WICBitmapInterpolationMode mode) const;

	[[nodiscard]] std::optional<std::string> encode(IWICBitmapSource* pixels, float quality) const;
	[[nodiscard]] ComPtr<IWICBitmapEncoder> create_encoder(IStream* stream) const;
	[[nodiscard]] bool write_frame(IWICBitmapEncoder* encoder, IWICBitmapSource* pixels, float quality) const;
	[[nodiscard]] bool set_quality(IPropertyBag2* options, float quality) const;

	static std::optional<GUID> container_of(Image::FormatEnum format);
	static std::optional<double> checked_pixel_count(IWICBitmapFrameDecode* frame);

	static USHORT read_exif_orientation(IWICBitmapFrameDecode* frame);
	static WICBitmapTransformOptions transform_of(USHORT exif_orientation);

	static bool set_frame_format(IWICBitmapFrameEncode* frame, IWICBitmapSource* pixels);
	static std::optional<std::string> read_stream(IStream* stream);

private:
	// NOTE: declared first so it is destroyed last, after every ComPtr below.
	utils::ComScope _com;
	std::string _file_name;
	GUID _container{};
	ComPtr<IWICImagingFactory> _factory;
	ComPtr<IWICBitmap> _pixels; // 24bppBGR for JPEG, 32bppBGRA for PNG
	double _decoded_pixel_ratio{1.0};
	WICBitmapTransformOptions _orientation{WICBitmapTransformRotate0};
};

} // namespace model

#endif // WINSPIALERTCLIENT_WICIMAGECOMPRESSOR_H
