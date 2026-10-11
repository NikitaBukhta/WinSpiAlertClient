#ifndef WINSPIALERTCLIENT_TESTS_IMAGECODEC_H
#define WINSPIALERTCLIENT_TESTS_IMAGECODEC_H

#include <array>
#include <format>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "utils/ComScope.h"

#define EXIF_ORIENTATION_QUERY L"/app1/ifd/{ushort=274}"
#define JPEG_QUALITY_OPTION L"ImageQuality"
#define FIRST_FRAME 0
#define NOISE_SEED 7
#define BGRA_BYTES 4
// a failed WIC call throws with its own text, so the message always matches the code
#define CHECK_HR(call) check((call), #call)

namespace test_helpers
{

struct EncodeOptions
{
	float jpeg_quality = 1.0f;
	USHORT orientation = 0; // EXIF orientation tag, 0 - none
};

struct DecodedImage
{
	UINT width{};
	UINT height{};
	GUID container{};
	GUID pixel_format{};
	std::vector<BYTE> bgra; // rows without padding
};

class ImageCodec
{
public:
	ImageCodec()
	{
		CHECK_HR(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_factory)));
	}

	[[nodiscard]] std::string noise(const GUID& container, const GUID& pixel_format, UINT width, UINT height,
	                                const EncodeOptions& options = {}) const
	{
		return encode(container, pixel_format, width, height, noise_pixels(pixel_format, width, height), options);
	}

	[[nodiscard]] std::vector<BYTE> noise_pixels(const GUID& pixel_format, UINT width, UINT height) const
	{
		std::vector<BYTE> pixels(static_cast<size_t>(stride(pixel_format, width)) * height);
		std::mt19937 random(NOISE_SEED);
		std::uniform_int_distribution<int> byte(0, 255);
		for (BYTE& value : pixels)
			value = static_cast<BYTE>(byte(random));
		return pixels;
	}

	[[nodiscard]] std::string encode(const GUID& container, const GUID& pixel_format, UINT width, UINT height,
	                                 std::span<const BYTE> pixels, const EncodeOptions& options = {}) const
	{
		const auto bitmap = create_bitmap(pixel_format, width, height, pixels);
		ComPtr<IStream> stream;
		ComPtr<IWICBitmapEncoder> encoder;
		CHECK_HR(CreateStreamOnHGlobal(nullptr, TRUE, &stream));
		CHECK_HR(_factory->CreateEncoder(container, nullptr, &encoder));
		CHECK_HR(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));

		write_frame(encoder.Get(), container, bitmap.Get(), options);
		CHECK_HR(encoder->Commit());
		return read_stream(stream.Get());
	}

	// nullopt if the data is not an image
	[[nodiscard]] std::optional<DecodedImage> decode(const std::string& data) const
	{
		const auto decoder = open_decoder(data);
		ComPtr<IWICBitmapFrameDecode> frame;
		if (!decoder || FAILED(decoder->GetFrame(FIRST_FRAME, &frame)))
			return std::nullopt;

		DecodedImage result;
		CHECK_HR(decoder->GetContainerFormat(&result.container));
		CHECK_HR(frame->GetSize(&result.width, &result.height));
		CHECK_HR(frame->GetPixelFormat(&result.pixel_format));
		result.bgra = read_bgra(frame.Get(), result.width, result.height);
		return result;
	}

	[[nodiscard]] UINT stride(const GUID& pixel_format, const UINT width) const
	{
		ComPtr<IWICComponentInfo> component;
		ComPtr<IWICPixelFormatInfo> info;
		UINT bits{};
		CHECK_HR(_factory->CreateComponentInfo(pixel_format, &component));
		CHECK_HR(component.As(&info));
		CHECK_HR(info->GetBitsPerPixel(&bits));
		return (width * bits + 7) / 8;
	}

	static std::array<double, BGRA_BYTES> average_bgra(const DecodedImage& image, const UINT x0, const UINT y0,
	                                                   const UINT x1, const UINT y1)
	{
		std::array<double, BGRA_BYTES> sum{};
		for (UINT y = y0; y < y1; ++y)
		{
			for (UINT x = x0; x < x1; ++x)
			{
				const size_t offset = (static_cast<size_t>(y) * image.width + x) * BGRA_BYTES;
				for (size_t channel = 0; channel < BGRA_BYTES; ++channel)
					sum[channel] += image.bgra[offset + channel];
			}
		}
		const double count = static_cast<double>(x1 - x0) * (y1 - y0);
		for (double& channel : sum)
			channel /= count;
		return sum;
	}

private:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	[[nodiscard]] ComPtr<IWICBitmap> create_bitmap(const GUID& pixel_format, UINT width, UINT height,
	                                               std::span<const BYTE> pixels) const
	{
		ComPtr<IWICBitmap> bitmap;
		auto* bytes = const_cast<BYTE*>(pixels.data());
		CHECK_HR(_factory->CreateBitmapFromMemory(width, height, pixel_format, stride(pixel_format, width),
		                                          static_cast<UINT>(pixels.size()), bytes, &bitmap));
		if (pixel_format == GUID_WICPixelFormat8bppIndexed)
			CHECK_HR(bitmap->SetPalette(halftone_palette().Get()));
		return bitmap;
	}

	[[nodiscard]] ComPtr<IWICPalette> halftone_palette() const
	{
		ComPtr<IWICPalette> palette;
		CHECK_HR(_factory->CreatePalette(&palette));
		CHECK_HR(palette->InitializePredefined(WICBitmapPaletteTypeFixedHalftone256, FALSE));
		return palette;
	}

	void write_frame(IWICBitmapEncoder* encoder, const GUID& container, IWICBitmap* bitmap,
	                 const EncodeOptions& options) const
	{
		ComPtr<IWICBitmapFrameEncode> frame;
		ComPtr<IPropertyBag2> bag;
		CHECK_HR(encoder->CreateNewFrame(&frame, &bag));
		if (container == GUID_ContainerFormatJpeg)
			set_jpeg_quality(bag.Get(), options.jpeg_quality);
		CHECK_HR(frame->Initialize(bag.Get()));

		UINT width{};
		UINT height{};
		GUID pixel_format{};
		CHECK_HR(bitmap->GetSize(&width, &height));
		CHECK_HR(bitmap->GetPixelFormat(&pixel_format));
		CHECK_HR(frame->SetSize(width, height));
		CHECK_HR(frame->SetPixelFormat(&pixel_format));
		if (pixel_format == GUID_WICPixelFormat8bppIndexed)
			CHECK_HR(frame->SetPalette(halftone_palette().Get()));
		if (options.orientation)
			write_orientation(frame.Get(), options.orientation);

		CHECK_HR(frame->WriteSource(bitmap, nullptr));
		CHECK_HR(frame->Commit());
	}

	// nullptr if the data is not an image
	[[nodiscard]] ComPtr<IWICBitmapDecoder> open_decoder(const std::string& data) const
	{
		ComPtr<IWICStream> stream;
		auto* bytes = reinterpret_cast<BYTE*>(const_cast<char*>(data.data()));
		CHECK_HR(_factory->CreateStream(&stream));
		CHECK_HR(stream->InitializeFromMemory(bytes, static_cast<DWORD>(data.size())));

		ComPtr<IWICBitmapDecoder> decoder;
		const HRESULT created =
			_factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
		return SUCCEEDED(created) ? decoder : nullptr;
	}

	[[nodiscard]] std::vector<BYTE> read_bgra(IWICBitmapSource* frame, const UINT width, const UINT height) const
	{
		// NOTE: keep straight alpha. Converting to BGR would blend away the color of transparent pixels.
		ComPtr<IWICFormatConverter> converter;
		CHECK_HR(_factory->CreateFormatConverter(&converter));
		CHECK_HR(converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
		                               WICBitmapPaletteTypeCustom));

		const UINT row = width * BGRA_BYTES;
		std::vector<BYTE> pixels(static_cast<size_t>(row) * height);
		CHECK_HR(converter->CopyPixels(nullptr, row, static_cast<UINT>(pixels.size()), pixels.data()));
		return pixels;
	}

	static void set_jpeg_quality(IPropertyBag2* bag, float quality)
	{
		PROPBAG2 option{};
		option.pstrName = const_cast<LPOLESTR>(JPEG_QUALITY_OPTION);
		VARIANT value{};
		value.vt = VT_R4;
		value.fltVal = quality;
		CHECK_HR(bag->Write(1, &option, &value));
	}

	static void write_orientation(IWICBitmapFrameEncode* frame, USHORT orientation)
	{
		ComPtr<IWICMetadataQueryWriter> writer;
		CHECK_HR(frame->GetMetadataQueryWriter(&writer));
		PROPVARIANT value;
		PropVariantInit(&value);
		value.vt = VT_UI2;
		value.uiVal = orientation;
		CHECK_HR(writer->SetMetadataByName(EXIF_ORIENTATION_QUERY, &value));
	}

	static std::string read_stream(IStream* stream)
	{
		STATSTG stat{};
		HGLOBAL memory{};
		CHECK_HR(stream->Stat(&stat, STATFLAG_NONAME));
		CHECK_HR(GetHGlobalFromStream(stream, &memory));
		const auto* bytes = static_cast<const char*>(GlobalLock(memory));
		if (!bytes)
			throw std::runtime_error("GlobalLock failed");

		std::string data(bytes, static_cast<size_t>(stat.cbSize.QuadPart));
		GlobalUnlock(memory);
		return data;
	}

	static void check(const HRESULT result, const char* call)
	{
		if (FAILED(result))
			throw std::runtime_error(std::format("{} failed: 0x{:08X}", call, static_cast<unsigned long>(result)));
	}

private:
	// NOTE: declared first so it is destroyed last, after the factory.
	utils::ComScope _com;
	ComPtr<IWICImagingFactory> _factory;
};

} // namespace test_helpers

#endif // WINSPIALERTCLIENT_TESTS_IMAGECODEC_H
