#include "WicImageCompressor.h"
#include "ImageConstants.h"

#include <algorithm>
#include <cmath>

#include <propkey.h>

namespace model
{

WicImageCompressor::WicImageCompressor(const Image& image) : _file_name(image.file_name())
{
	decode(image);
}

bool WicImageCompressor::is_decoded() const noexcept
{
	return _pixels != nullptr;
}

uint32_t WicImageCompressor::min_side() const
{
	UINT width{};
	UINT height{};
	if (!_pixels || FAILED(_pixels->GetSize(&width, &height)))
		return 0;

	return (std::min)(width, height);
}

double WicImageCompressor::decoded_pixel_ratio() const
{
	return _decoded_pixel_ratio;
}

std::optional<Image> WicImageCompressor::compress(double scale, float quality) const
{
	const auto pixels = transformed(scale);
	if (!pixels)
		return std::nullopt;

	auto data = encode(pixels.Get(), quality);
	if (!data)
		return std::nullopt;

	return Image(_file_name, std::move(*data));
}

void WicImageCompressor::decode(const Image& image)
{
	const auto format = image.format();
	if (!format)
		return;
	const auto container = container_of(*format);
	if (!container)
		return;
	_container = *container;

	const bool can_decode = _com.is_initialized() && create_factory();
	if (!can_decode)
		return;

	const auto frame = open_frame(image);
	if (!frame)
		return;

	const auto pixel_count = checked_pixel_count(frame.Get());
	if (!pixel_count)
		return;

	// NOTE: a 20 MB JPEG takes hundreds of MB once decoded, so a large image is shrunk right away.
	// Fant reads the decoder row by row; the sharper cubic is used later on the cached pixels.
	const double decode_scale = (std::min)(1.0, std::sqrt(MAX_DECODE_PIXELS / *pixel_count));
	auto source = to_encoder_format(frame.Get());
	if (source && decode_scale < 1.0)
		source = scaled(source.Get(), decode_scale, WICBitmapInterpolationModeFant);
	if (!source || FAILED(_factory->CreateBitmapFromSource(source.Get(), WICBitmapCacheOnLoad, &_pixels)))
		return;

	_decoded_pixel_ratio = decode_scale * decode_scale;
	// NOTE: EXIF is not copied to the result, so the pixels are rotated instead.
	_orientation = transform_of(read_exif_orientation(frame.Get()));
}

bool WicImageCompressor::create_factory()
{
	const HRESULT created =
		CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_factory));
	return SUCCEEDED(created);
}

WicImageCompressor::ComPtr<IWICBitmapFrameDecode> WicImageCompressor::open_frame(const Image& image) const
{
	ComPtr<IWICStream> stream;
	if (FAILED(_factory->CreateStream(&stream)))
		return nullptr;
	// NOTE: WIC only reads the buffer.
	auto* bytes = reinterpret_cast<BYTE*>(const_cast<char*>(image.data().data()));
	if (FAILED(stream->InitializeFromMemory(bytes, static_cast<DWORD>(image.size()))))
		return nullptr;

	ComPtr<IWICBitmapDecoder> decoder;
	ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(_factory->CreateDecoder(_container, nullptr, &decoder)))
		return nullptr;
	if (FAILED(decoder->Initialize(stream.Get(), WICDecodeMetadataCacheOnDemand)))
		return nullptr;
	if (FAILED(decoder->GetFrame(FIRST_FRAME, &frame)))
		return nullptr;

	return frame;
}

WicImageCompressor::ComPtr<IWICBitmapSource> WicImageCompressor::to_encoder_format(IWICBitmapSource* source) const
{
	const bool is_jpeg = _container == GUID_ContainerFormatJpeg;
	const GUID pixel_format = is_jpeg ? GUID_WICPixelFormat24bppBGR : GUID_WICPixelFormat32bppBGRA;
	ComPtr<IWICFormatConverter> converter;
	if (FAILED(_factory->CreateFormatConverter(&converter)))
		return nullptr;

	const HRESULT initialized =
		converter->Initialize(source, pixel_format, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
	if (FAILED(initialized))
		return nullptr;

	return converter;
}

WicImageCompressor::ComPtr<IWICBitmapSource> WicImageCompressor::transformed(double scale) const
{
	if (!_pixels)
		return nullptr;

	// NOTE: the rotator reads its source in random order, so it runs on the cached bitmap before scaling.
	auto pixels = rotated(_pixels.Get());
	if (!pixels || scale >= 1.0)
		return pixels;

	return scaled(pixels.Get(), scale, WICBitmapInterpolationModeHighQualityCubic);
}

WicImageCompressor::ComPtr<IWICBitmapSource> WicImageCompressor::rotated(IWICBitmapSource* source) const
{
	if (_orientation == WICBitmapTransformRotate0)
		return source;

	ComPtr<IWICBitmapFlipRotator> rotator;
	if (FAILED(_factory->CreateBitmapFlipRotator(&rotator)))
		return nullptr;
	if (FAILED(rotator->Initialize(source, _orientation)))
		return nullptr;

	return rotator;
}

WicImageCompressor::ComPtr<IWICBitmapSource> WicImageCompressor::scaled(IWICBitmapSource* source, double scale,
                                                                        WICBitmapInterpolationMode mode) const
{
	UINT width{};
	UINT height{};
	if (FAILED(source->GetSize(&width, &height)))
		return nullptr;

	const auto scaled_width = (std::max)(1u, static_cast<UINT>(std::lround(width * scale)));
	const auto scaled_height = (std::max)(1u, static_cast<UINT>(std::lround(height * scale)));
	ComPtr<IWICBitmapScaler> scaler;
	if (FAILED(_factory->CreateBitmapScaler(&scaler)))
		return nullptr;
	if (FAILED(scaler->Initialize(source, scaled_width, scaled_height, mode)))
		return nullptr;

	return scaler;
}

std::optional<std::string> WicImageCompressor::encode(IWICBitmapSource* pixels, float quality) const
{
	ComPtr<IStream> stream;
	if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)))
		return std::nullopt;

	const auto encoder = create_encoder(stream.Get());
	const bool encoded = encoder && write_frame(encoder.Get(), pixels, quality) && SUCCEEDED(encoder->Commit());
	if (!encoded)
		return std::nullopt;

	return read_stream(stream.Get());
}

WicImageCompressor::ComPtr<IWICBitmapEncoder> WicImageCompressor::create_encoder(IStream* stream) const
{
	ComPtr<IWICBitmapEncoder> encoder;
	if (FAILED(_factory->CreateEncoder(_container, nullptr, &encoder)))
		return nullptr;
	if (FAILED(encoder->Initialize(stream, WICBitmapEncoderNoCache)))
		return nullptr;

	return encoder;
}

bool WicImageCompressor::write_frame(IWICBitmapEncoder* encoder, IWICBitmapSource* pixels, float quality) const
{
	ComPtr<IWICBitmapFrameEncode> frame;
	ComPtr<IPropertyBag2> options;
	if (FAILED(encoder->CreateNewFrame(&frame, &options)))
		return false;
	if (!set_quality(options.Get(), quality) || FAILED(frame->Initialize(options.Get())))
		return false;
	if (!set_frame_format(frame.Get(), pixels))
		return false;

	return SUCCEEDED(frame->WriteSource(pixels, nullptr)) && SUCCEEDED(frame->Commit());
}

bool WicImageCompressor::set_quality(IPropertyBag2* options, float quality) const
{
	if (_container != GUID_ContainerFormatJpeg)
		return true;

	PROPBAG2 option{};
	option.pstrName = const_cast<LPOLESTR>(JPEG_QUALITY_OPTION);
	VARIANT value{};
	value.vt = VT_R4;
	value.fltVal = quality;
	return SUCCEEDED(options->Write(1, &option, &value));
}

std::optional<GUID> WicImageCompressor::container_of(Image::FormatEnum format)
{
	switch (format)
	{
	case Image::FormatEnum::JPEG_FORMAT:
		return GUID_ContainerFormatJpeg;
	case Image::FormatEnum::PNG_FORMAT:
		return GUID_ContainerFormatPng;
	}
	return std::nullopt;
}

std::optional<double> WicImageCompressor::checked_pixel_count(IWICBitmapFrameDecode* frame)
{
	UINT width{};
	UINT height{};
	if (FAILED(frame->GetSize(&width, &height)))
		return std::nullopt;

	const double pixel_count = static_cast<double>(width) * height;
	if (pixel_count == 0 || pixel_count > MAX_SOURCE_PIXELS)
		return std::nullopt;

	return pixel_count;
}

USHORT WicImageCompressor::read_exif_orientation(IWICBitmapFrameDecode* frame)
{
	ComPtr<IWICMetadataQueryReader> reader;
	if (FAILED(frame->GetMetadataQueryReader(&reader)))
		return PHOTO_ORIENTATION_NORMAL;

	PROPVARIANT value;
	PropVariantInit(&value);
	const HRESULT read = reader->GetMetadataByName(EXIF_ORIENTATION_QUERY, &value);
	const bool has_orientation = SUCCEEDED(read) && value.vt == VT_UI2;
	const USHORT orientation = has_orientation ? value.uiVal : PHOTO_ORIENTATION_NORMAL;
	PropVariantClear(&value);
	return orientation;
}

WICBitmapTransformOptions WicImageCompressor::transform_of(USHORT exif_orientation)
{
	// NOTE: the tag says how the stored image is turned, so ROTATE270 is undone by 90 degrees clockwise.
	// WIC flips before it rotates.
	switch (exif_orientation)
	{
	case PHOTO_ORIENTATION_FLIPHORIZONTAL:
		return WICBitmapTransformFlipHorizontal;
	case PHOTO_ORIENTATION_ROTATE180:
		return WICBitmapTransformRotate180;
	case PHOTO_ORIENTATION_FLIPVERTICAL:
		return WICBitmapTransformFlipVertical;
	case PHOTO_ORIENTATION_TRANSPOSE:
		return static_cast<WICBitmapTransformOptions>(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal);
	case PHOTO_ORIENTATION_ROTATE270:
		return WICBitmapTransformRotate90;
	case PHOTO_ORIENTATION_TRANSVERSE:
		return static_cast<WICBitmapTransformOptions>(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal);
	case PHOTO_ORIENTATION_ROTATE90:
		return WICBitmapTransformRotate270;
	default:
		return WICBitmapTransformRotate0;
	}
}

bool WicImageCompressor::set_frame_format(IWICBitmapFrameEncode* frame, IWICBitmapSource* pixels)
{
	UINT width{};
	UINT height{};
	GUID pixel_format{};
	if (FAILED(pixels->GetSize(&width, &height)) || FAILED(pixels->GetPixelFormat(&pixel_format)))
		return false;

	return SUCCEEDED(frame->SetSize(width, height)) && SUCCEEDED(frame->SetPixelFormat(&pixel_format));
}

std::optional<std::string> WicImageCompressor::read_stream(IStream* stream)
{
	STATSTG stat{};
	HGLOBAL memory{};
	if (FAILED(stream->Stat(&stat, STATFLAG_NONAME)) || FAILED(GetHGlobalFromStream(stream, &memory)))
		return std::nullopt;

	const void* bytes = GlobalLock(memory);
	if (!bytes)
		return std::nullopt;

	std::string data(static_cast<const char*>(bytes), static_cast<size_t>(stat.cbSize.QuadPart));
	GlobalUnlock(memory);
	return data;
}

} // namespace model
