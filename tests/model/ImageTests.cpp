#include <gtest/gtest.h>

#include <string>

#include "model/Image.h"
#include "tests/helpers/TempFile.h"

#define PNG_DATA "\x89PNG\r\n\x1a\nimage"
#define JPEG_DATA "\xff\xd8\xff\xe0image"

#define MAX_LOAD_SIZE 1024

using namespace std::string_literals;
using model::Image;
using test_helpers::TempFile;

TEST(ImageTests, KeepsFileNameAndData)
{
	const Image image("a.png", "\x89PNG\0\xff"s);

	EXPECT_EQ(image.file_name(), "a.png");
	EXPECT_EQ(image.data(), "\x89PNG\0\xff"s);
	EXPECT_EQ(image.size(), 6u);
}

TEST(ImageTests, LoadsFileNameWithoutDirectoryAndBytes)
{
	const std::string content = "\x89PNG\r\n\x1a\n\0\xff"s;
	const TempFile file("capture_0001.png", content);

	const auto image = Image::load(file.path(), MAX_LOAD_SIZE);

	ASSERT_TRUE(image.has_value());
	EXPECT_EQ(image->file_name(), "capture_0001.png");
	EXPECT_EQ(image->data(), content);
	EXPECT_EQ(image->size(), content.size());
}

TEST(ImageTests, LoadsEmptyFile)
{
	const TempFile file("a.png", "");

	const auto image = Image::load(file.path(), MAX_LOAD_SIZE);

	ASSERT_TRUE(image.has_value());
	EXPECT_EQ(image->size(), 0u);
}

TEST(ImageTests, MissingFileIsNotLoaded)
{
	EXPECT_FALSE(Image::load("Z:\\no\\such\\a.png", MAX_LOAD_SIZE).has_value());
}

TEST(ImageTests, ContentTypeOfSupportedFormats)
{
	EXPECT_EQ(Image("a.png", PNG_DATA).content_type(), "image/png");
	EXPECT_EQ(Image("a.jpg", JPEG_DATA).content_type(), "image/jpeg");
	EXPECT_EQ(Image("a.jpeg", JPEG_DATA).content_type(), "image/jpeg");
}

TEST(ImageTests, UnsupportedExtensionHasNoContentType)
{
	EXPECT_FALSE(Image("a.gif", "GIF89a").content_type().has_value());
	EXPECT_FALSE(Image("a.bmp", PNG_DATA).content_type().has_value());
	EXPECT_FALSE(Image("png", PNG_DATA).content_type().has_value());
	EXPECT_FALSE(Image("a.png.txt", PNG_DATA).content_type().has_value());
}

TEST(ImageTests, UpperCaseExtensionIsNotSupported)
{
	// the server accepts lower case extensions only
	EXPECT_FALSE(Image("a.PNG", PNG_DATA).content_type().has_value());
}

TEST(ImageTests, ContentNotMatchingExtensionHasNoContentType)
{
	EXPECT_FALSE(Image("a.png", JPEG_DATA).content_type().has_value());
	EXPECT_FALSE(Image("a.jpg", PNG_DATA).content_type().has_value());
	EXPECT_FALSE(Image("a.png", "").content_type().has_value());
	EXPECT_FALSE(Image("a.png", "\x89PNG").content_type().has_value());
}

TEST(ImageTests, ValidImageHasNoValidationError)
{
	EXPECT_FALSE(Image("a.png", PNG_DATA).validation_error().has_value());
	EXPECT_FALSE(Image("a.jpeg", JPEG_DATA).validation_error().has_value());
}

TEST(ImageTests, ValidationErrorNamesSupportedExtensions)
{
	EXPECT_EQ(Image("a.gif", "GIF89a").validation_error(), "a.gif: unsupported extension, expected .png, .jpg, .jpeg");
	EXPECT_EQ(Image(".png", PNG_DATA).validation_error(), ".png: unsupported extension, expected .png, .jpg, .jpeg");
}

TEST(ImageTests, ValidationErrorTellsContentMismatch)
{
	EXPECT_EQ(Image("a.png", JPEG_DATA).validation_error(), "a.png: content does not match the .png extension");
}

TEST(ImageTests, LoadsPathOutsideTheAnsiCodePage)
{
	// file name in UTF-8: Cyrillic and CJK characters, which no single ANSI code page holds together
	const std::u8string utf8_name = u8"фото_写真.png";
	const TempFile file(std::filesystem::path(utf8_name), PNG_DATA);

	const auto image = Image::load(file.path(), MAX_LOAD_SIZE);

	ASSERT_TRUE(image.has_value());
	EXPECT_EQ(image->file_name(), std::string(utf8_name.begin(), utf8_name.end()));
	EXPECT_TRUE(image->validation_error().has_value()) << "loads fine, but the server accepts ASCII names only";
}

TEST(ImageTests, NameWithAllowedCharactersIsValid)
{
	EXPECT_EQ(Image("capture_0001-A.png", PNG_DATA).content_type(), "image/png");
	EXPECT_EQ(Image(std::string(100, 'a') + ".png", PNG_DATA).content_type(), "image/png");
}

TEST(ImageTests, NameOutsideServerRulesIsRejected)
{
	EXPECT_FALSE(Image("class-diagram.drawio.png", PNG_DATA).content_type().has_value()) << "dot inside the name";
	EXPECT_FALSE(Image("capture 0001.png", PNG_DATA).content_type().has_value()) << "space";
	EXPECT_FALSE(Image("capture+1.png", PNG_DATA).content_type().has_value());
	EXPECT_FALSE(Image(std::string(101, 'a') + ".png", PNG_DATA).content_type().has_value()) << "101 characters";
}

TEST(ImageTests, ValidationErrorExplainsNameRules)
{
	EXPECT_EQ(Image("class-diagram.drawio.png", PNG_DATA).validation_error(),
	          "class-diagram.drawio.png: the name before the extension must be 1-100 characters from A-Z a-z 0-9 _ -");
}

TEST(ImageTests, FileLargerThanMaxSizeIsNotLoaded)
{
	const TempFile file("a.png", std::string(MAX_LOAD_SIZE + 1, 'x'));

	EXPECT_FALSE(Image::load(file.path(), MAX_LOAD_SIZE).has_value());
}
