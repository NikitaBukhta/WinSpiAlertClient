#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "model/Image.h"
#include "model/WicImageCompressor.h"
#include "model/WicImageCompressorFactory.h"
#include "tests/helpers/ImageCodec.h"

#define WIDTH 480
#define HEIGHT 320
#define MAX_DECODE_PIXELS (24 * 1000 * 1000)
#define MARK_PART 4
#define PROBE_PART 10
#define MARK_CHANNEL 160
#define BROKEN_PNG_DATA "\x89PNG\r\n\x1a\nbroken"

using model::Image;
using model::WicImageCompressor;
using model::WicImageCompressorFactory;
using test_helpers::DecodedImage;
using test_helpers::EncodeOptions;
using test_helpers::ImageCodec;

namespace
{

enum class CornerEnum : uint8_t
{
	TOP_LEFT_CORNER,
	TOP_RIGHT_CORNER,
	BOTTOM_LEFT_CORNER,
	BOTTOM_RIGHT_CORNER,
};

struct OrientationCase
{
	USHORT orientation;
	CornerEnum mark;
	bool sides_swapped;
};

Image noise_jpeg(const EncodeOptions& options = {}, const GUID& pixel_format = GUID_WICPixelFormat24bppBGR)
{
	return {"capture_0001.jpg", ImageCodec().noise(GUID_ContainerFormatJpeg, pixel_format, WIDTH, HEIGHT, options)};
}

Image noise_png(const GUID& pixel_format, const EncodeOptions& options = {})
{
	return {"capture_0001.png", ImageCodec().noise(GUID_ContainerFormatPng, pixel_format, WIDTH, HEIGHT, options)};
}

// a noise JPEG with a red mark in the top left corner of the stored pixels
Image marked_jpeg(USHORT orientation)
{
	auto pixels = ImageCodec().noise_pixels(GUID_WICPixelFormat24bppBGR, WIDTH, HEIGHT);
	for (UINT y = 0; y < HEIGHT / MARK_PART; ++y)
	{
		for (UINT x = 0; x < WIDTH / MARK_PART; ++x)
		{
			const size_t offset = (static_cast<size_t>(y) * WIDTH + x) * 3;
			pixels[offset] = 0;
			pixels[offset + 1] = 0;
			pixels[offset + 2] = 255;
		}
	}
	EncodeOptions options;
	options.orientation = orientation;
	return {"oriented.jpg",
	        ImageCodec().encode(GUID_ContainerFormatJpeg, GUID_WICPixelFormat24bppBGR, WIDTH, HEIGHT, pixels, options)};
}

bool is_red(const DecodedImage& image, CornerEnum corner)
{
	const UINT probe_width = image.width / PROBE_PART;
	const UINT probe_height = image.height / PROBE_PART;
	const bool right = corner == CornerEnum::TOP_RIGHT_CORNER || corner == CornerEnum::BOTTOM_RIGHT_CORNER;
	const bool bottom = corner == CornerEnum::BOTTOM_LEFT_CORNER || corner == CornerEnum::BOTTOM_RIGHT_CORNER;
	const UINT x0 = right ? image.width - probe_width : 0;
	const UINT y0 = bottom ? image.height - probe_height : 0;
	const auto [blue, green, red, alpha] = ImageCodec::average_bgra(image, x0, y0, x0 + probe_width, y0 + probe_height);
	return red > MARK_CHANNEL && green < 255 - MARK_CHANNEL && blue < 255 - MARK_CHANNEL;
}

// the result keeps the name, the format and the signature, and decodes
DecodedImage expect_compressed(const std::optional<Image>& compressed, const Image& source, const GUID& container)
{
	EXPECT_TRUE(compressed.has_value());
	if (!compressed)
		return {};

	EXPECT_EQ(compressed->file_name(), source.file_name());
	EXPECT_EQ(compressed->format(), source.format());
	EXPECT_EQ(compressed->validation_error(), std::nullopt);
	const auto decoded = ImageCodec().decode(compressed->data());
	EXPECT_TRUE(decoded.has_value());
	if (!decoded)
		return {};

	EXPECT_EQ(decoded->container, container);
	return *decoded;
}

class WicImageCompressorOrientationTests : public testing::TestWithParam<OrientationCase>
{
};

} // namespace

TEST(WicImageCompressorTests, JpegStaysJpegOfTheSameSize)
{
	const Image source = noise_jpeg();
	const WicImageCompressor compressor(source);

	const auto compressed = compressor.compress(1.0, 0.75f);

	const auto decoded = expect_compressed(compressed, source, GUID_ContainerFormatJpeg);
	EXPECT_LT(compressed->size(), source.size());
	EXPECT_EQ(decoded.width, WIDTH);
	EXPECT_EQ(decoded.height, HEIGHT);
}

TEST(WicImageCompressorTests, LowerQualityGivesSmallerJpeg)
{
	const WicImageCompressor compressor(noise_jpeg());

	const auto high = compressor.compress(1.0, 0.9f);
	const auto low = compressor.compress(1.0, 0.6f);

	ASSERT_TRUE(high.has_value());
	ASSERT_TRUE(low.has_value());
	EXPECT_LT(low->size(), high->size());
}

TEST(WicImageCompressorTests, ScaleShrinksBothSides)
{
	const Image source = noise_jpeg();
	const WicImageCompressor compressor(source);

	const auto compressed = compressor.compress(0.5, 0.75f);

	const auto decoded = expect_compressed(compressed, source, GUID_ContainerFormatJpeg);
	EXPECT_EQ(decoded.width, WIDTH / 2);
	EXPECT_EQ(decoded.height, HEIGHT / 2);
}

TEST(WicImageCompressorTests, MinimumScaleGivesExactly640Pixels)
{
	// 640.0 / 1077 * 1077 is 639.99... in double, so rounding down would give 639 px
	const Image source("capture_0001.jpg",
	                   ImageCodec().noise(GUID_ContainerFormatJpeg, GUID_WICPixelFormat24bppBGR, 1600, 1077));
	const WicImageCompressor compressor(source);

	const auto compressed = compressor.compress(640.0 / 1077, 0.6f);

	const auto decoded = expect_compressed(compressed, source, GUID_ContainerFormatJpeg);
	EXPECT_EQ(decoded.height, 640u);
}

TEST(WicImageCompressorTests, MinSideIsTheShorterSide)
{
	const WicImageCompressor compressor(noise_jpeg());

	EXPECT_EQ(compressor.min_side(), HEIGHT);
}

TEST(WicImageCompressorTests, PngStaysPngWithAlpha)
{
	const Image source = noise_png(GUID_WICPixelFormat32bppBGRA);
	const WicImageCompressor compressor(source);

	const auto compressed = compressor.compress(0.5, 0.75f);

	const auto decoded = expect_compressed(compressed, source, GUID_ContainerFormatPng);
	EXPECT_EQ(decoded.pixel_format, GUID_WICPixelFormat32bppBGRA);
	EXPECT_EQ(decoded.width, WIDTH / 2);
}

TEST(WicImageCompressorTests, PalettePngIsScaledAsBgra)
{
	const Image source = noise_png(GUID_WICPixelFormat8bppIndexed);
	const WicImageCompressor compressor(source);

	const auto compressed = compressor.compress(0.5, 0.75f);

	const auto decoded = expect_compressed(compressed, source, GUID_ContainerFormatPng);
	EXPECT_EQ(decoded.pixel_format, GUID_WICPixelFormat32bppBGRA);
}

TEST(WicImageCompressorTests, CmykJpegIsCompressedAsBgr)
{
	const Image source = noise_jpeg({}, GUID_WICPixelFormat32bppCMYK);
	const WicImageCompressor compressor(source);

	const auto compressed = compressor.compress(0.5, 0.75f);

	const auto decoded = expect_compressed(compressed, source, GUID_ContainerFormatJpeg);
	EXPECT_EQ(decoded.pixel_format, GUID_WICPixelFormat24bppBGR);
}

TEST(WicImageCompressorTests, GrayJpegIsCompressedAsBgr)
{
	const Image source = noise_jpeg({}, GUID_WICPixelFormat8bppGray);
	const WicImageCompressor compressor(source);

	const auto compressed = compressor.compress(0.5, 0.75f);

	const auto decoded = expect_compressed(compressed, source, GUID_ContainerFormatJpeg);
	EXPECT_EQ(decoded.pixel_format, GUID_WICPixelFormat24bppBGR);
}

TEST(WicImageCompressorTests, HighResolutionImageIsShrunkWhileDecoded)
{
	const Image source("wide.png", ImageCodec().encode(GUID_ContainerFormatPng, GUID_WICPixelFormatBlackWhite, 6000,
	                                                   5000, std::vector<BYTE>(750 * 5000)));
	const WicImageCompressor compressor(source);

	const auto compressed = compressor.compress(1.0, 0.75f);

	const auto decoded = expect_compressed(compressed, source, GUID_ContainerFormatPng);
	EXPECT_NEAR(static_cast<double>(decoded.width) * decoded.height, MAX_DECODE_PIXELS, MAX_DECODE_PIXELS * 0.01);
	EXPECT_NEAR(compressor.decoded_pixel_ratio(), MAX_DECODE_PIXELS / (6000.0 * 5000), 1e-6);
}

TEST(WicImageCompressorTests, ImageDecodedWholeKeepsAllPixels)
{
	const WicImageCompressor compressor(noise_jpeg());

	EXPECT_DOUBLE_EQ(compressor.decoded_pixel_ratio(), 1.0);
}

TEST(WicImageCompressorTests, ImageWithTooManyPixelsIsNotDecoded)
{
	// 404 megapixels of zeros, a decompression bomb
	const UINT side = 20100;
	const std::vector<BYTE> zeros(static_cast<size_t>(ImageCodec().stride(GUID_WICPixelFormatBlackWhite, side)) * side);
	const Image source("bomb.png",
	                   ImageCodec().encode(GUID_ContainerFormatPng, GUID_WICPixelFormatBlackWhite, side, side, zeros));

	const WicImageCompressor compressor(source);

	EXPECT_FALSE(compressor.is_decoded());
	EXPECT_EQ(compressor.compress(0.5, 0.75f), std::nullopt);
}

TEST(WicImageCompressorTests, BrokenImageIsNotDecoded)
{
	const WicImageCompressor compressor(Image("broken.png", BROKEN_PNG_DATA));

	EXPECT_FALSE(compressor.is_decoded());
	EXPECT_EQ(compressor.min_side(), 0u);
	EXPECT_EQ(compressor.compress(1.0, 0.75f), std::nullopt);
}

TEST(WicImageCompressorTests, UnsupportedImageIsNotDecoded)
{
	const WicImageCompressor compressor(Image("capture.gif", "GIF89a"));

	EXPECT_FALSE(compressor.is_decoded());
}

TEST(WicImageCompressorFactoryTests, BrokenImageGetsNoCompressor)
{
	EXPECT_EQ(WicImageCompressorFactory().create(Image("broken.png", BROKEN_PNG_DATA)), nullptr);
}

TEST(WicImageCompressorFactoryTests, DecodableImageGetsCompressor)
{
	const auto compressor = WicImageCompressorFactory().create(noise_jpeg());

	ASSERT_NE(compressor, nullptr);
	EXPECT_EQ(compressor->min_side(), HEIGHT);
}

TEST_P(WicImageCompressorOrientationTests, ResultIsTurnedUprightByExifOrientation)
{
	const auto [orientation, mark, sides_swapped] = GetParam();
	const Image source = marked_jpeg(orientation);
	const WicImageCompressor compressor(source);

	const auto compressed = compressor.compress(0.5, 0.75f);

	const auto decoded = expect_compressed(compressed, source, GUID_ContainerFormatJpeg);
	EXPECT_EQ(decoded.width < decoded.height, sides_swapped);
	EXPECT_TRUE(is_red(decoded, mark));
}

INSTANTIATE_TEST_SUITE_P(WicImageCompressorTests, WicImageCompressorOrientationTests,
                         testing::Values(OrientationCase{1, CornerEnum::TOP_LEFT_CORNER, false},
                                         OrientationCase{2, CornerEnum::TOP_RIGHT_CORNER, false},
                                         OrientationCase{3, CornerEnum::BOTTOM_RIGHT_CORNER, false},
                                         OrientationCase{4, CornerEnum::BOTTOM_LEFT_CORNER, false},
                                         OrientationCase{5, CornerEnum::TOP_LEFT_CORNER, true},
                                         OrientationCase{6, CornerEnum::TOP_RIGHT_CORNER, true},
                                         OrientationCase{7, CornerEnum::BOTTOM_RIGHT_CORNER, true},
                                         OrientationCase{8, CornerEnum::BOTTOM_LEFT_CORNER, true}));
