#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "model/Image.h"
#include "model/ImageSizePredictor.h"
#include "tests/helpers/TestConstants.h"

#define KB 1000
#define MAX_SIZE (5 * KB)
#define ALL_PIXELS 1.0

using model::Image;
using model::ImageSizePredictor;

namespace
{

Image jpeg(size_t size)
{
	const std::string signature = JPEG_SIGNATURE;
	return Image("capture.jpg", signature + std::string(size - signature.size(), 'x'));
}

Image png(size_t size)
{
	const std::string signature = PNG_SIGNATURE;
	return Image("capture.png", signature + std::string(size - signature.size(), 'x'));
}

} // namespace

TEST(ImageSizePredictorTests, JpegSlightlyOverTheLimitLowersQualityOnly)
{
	ImageSizePredictor predictor(jpeg(6 * KB), MAX_SIZE, LARGE_SIDE, ALL_PIXELS);

	const auto step = predictor.next_step();

	ASSERT_TRUE(step.has_value());
	EXPECT_DOUBLE_EQ(step->scale, 1.0);
	EXPECT_FLOAT_EQ(step->quality, 0.75f);
}

TEST(ImageSizePredictorTests, JpegThatDoesNotFitAtMinimumQualityIsScaled)
{
	ImageSizePredictor predictor(jpeg(6 * KB), MAX_SIZE, LARGE_SIDE, ALL_PIXELS);
	predictor.next_step();
	predictor.add_measurement(6 * KB);

	const auto step = predictor.next_step();

	// sqrt(5 / 6) with the 0.9 margin
	ASSERT_TRUE(step.has_value());
	EXPECT_NEAR(step->scale, 0.8216, 1e-4);
	EXPECT_FLOAT_EQ(step->quality, 0.75f);
}

TEST(ImageSizePredictorTests, JpegFarOverTheLimitStartsWithScale)
{
	ImageSizePredictor predictor(jpeg(20 * KB), MAX_SIZE, LARGE_SIDE, ALL_PIXELS);

	const auto step = predictor.next_step();

	// sqrt(5 / 20) with the 0.9 margin
	ASSERT_TRUE(step.has_value());
	EXPECT_DOUBLE_EQ(step->scale, 0.45);
	EXPECT_FLOAT_EQ(step->quality, 0.75f);
}

TEST(ImageSizePredictorTests, PngSlightlyOverTheLimitStartsWithScale)
{
	ImageSizePredictor predictor(png(11 * KB / 2), MAX_SIZE, LARGE_SIDE, ALL_PIXELS);

	const auto step = predictor.next_step();

	// no re-encoding as is: sqrt(5 / 5.5) with the 0.9 margin
	ASSERT_TRUE(step.has_value());
	EXPECT_NEAR(step->scale, 0.8581, 1e-4);
}

TEST(ImageSizePredictorTests, ScaleIsRelativeToTheMeasuredOne)
{
	ImageSizePredictor predictor(png(20 * KB), MAX_SIZE, LARGE_SIDE, ALL_PIXELS);
	const double first_scale = predictor.next_step()->scale;
	predictor.add_measurement(20 * KB);

	const auto step = predictor.next_step();

	// 4x over: half of each side with the 0.9 margin, twice
	EXPECT_DOUBLE_EQ(first_scale, 0.45);
	ASSERT_TRUE(step.has_value());
	EXPECT_DOUBLE_EQ(step->scale, 0.45 * 0.45);
}

TEST(ImageSizePredictorTests, ScaleCountsOnlyThePixelsLeftAfterDecoding)
{
	// a half of the pixels was dropped while decoding: 20 KB of the source is about 10 KB at scale 1.0
	ImageSizePredictor predictor(png(20 * KB), MAX_SIZE, LARGE_SIDE, 0.5);

	const auto step = predictor.next_step();

	// sqrt(5 / 10) with the 0.9 margin
	ASSERT_TRUE(step.has_value());
	EXPECT_NEAR(step->scale, 0.6364, 1e-4);
}

TEST(ImageSizePredictorTests, JpegThatFitsAfterDecodingLowersQualityOnly)
{
	// 10 KB is 2x over the limit, but the decoded half of it is 1.0x
	ImageSizePredictor predictor(jpeg(10 * KB), MAX_SIZE, LARGE_SIDE, 0.5);

	const auto step = predictor.next_step();

	ASSERT_TRUE(step.has_value());
	EXPECT_DOUBLE_EQ(step->scale, 1.0);
	EXPECT_FLOAT_EQ(step->quality, 0.75f);
}

TEST(ImageSizePredictorTests, ScaleBelowTheMinimumSideGoesToTheLastStep)
{
	// 640 px of 1280 px: the minimum scale 0.5 is above the predicted 0.45
	ImageSizePredictor predictor(png(20 * KB), MAX_SIZE, 1280, ALL_PIXELS);

	const auto step = predictor.next_step();

	ASSERT_TRUE(step.has_value());
	EXPECT_DOUBLE_EQ(step->scale, 0.5);
	EXPECT_FLOAT_EQ(step->quality, 0.6f);
	EXPECT_EQ(predictor.next_step(), std::nullopt);
}

TEST(ImageSizePredictorTests, SmallImageGetsOnlyTheLastStep)
{
	ImageSizePredictor predictor(jpeg(10 * KB), MAX_SIZE, 320, ALL_PIXELS);

	const auto step = predictor.next_step();

	ASSERT_TRUE(step.has_value());
	EXPECT_DOUBLE_EQ(step->scale, 1.0);
	EXPECT_FLOAT_EQ(step->quality, 0.6f);
	EXPECT_EQ(predictor.next_step(), std::nullopt);
}

TEST(ImageSizePredictorTests, AtMostFourStepsEndingWithTheMinimum)
{
	ImageSizePredictor predictor(jpeg(6 * KB), MAX_SIZE, 6400, ALL_PIXELS);

	int steps = 0;
	std::optional<ImageSizePredictor::Step> last;
	for (auto step = predictor.next_step(); step; step = predictor.next_step())
	{
		++steps;
		last = step;
		predictor.add_measurement(6 * KB);
	}

	EXPECT_EQ(steps, 4);
	ASSERT_TRUE(last.has_value());
	EXPECT_DOUBLE_EQ(last->scale, 0.1);
	EXPECT_FLOAT_EQ(last->quality, 0.6f);
}
