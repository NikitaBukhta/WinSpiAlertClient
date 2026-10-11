#include <gtest/gtest.h>

#include <string>

#include "utils/SizeUnits.h"
#include "utils/StringUtils.h"

using utils::StringUtils;

TEST(StringUtilsTests, ConvertsAsciiString)
{
	EXPECT_EQ(StringUtils::to_wide("challenge.spialert.com"), L"challenge.spialert.com");
}

TEST(StringUtilsTests, ConvertsEmptyString)
{
	EXPECT_EQ(StringUtils::to_wide(""), L"");
}

TEST(StringUtilsTests, KeepsEveryAsciiCharacter)
{
	std::string ascii;
	for (int ch = 1; ch < 128; ++ch)
		ascii.push_back(static_cast<char>(ch));

	const std::wstring wide = StringUtils::to_wide(ascii);

	ASSERT_EQ(wide.size(), ascii.size());
	for (size_t i = 0; i < ascii.size(); ++i)
		EXPECT_EQ(wide[i], static_cast<wchar_t>(ascii[i])) << "index " << i;
}

TEST(StringUtilsTests, KeepsHeaderSeparators)
{
	EXPECT_EQ(StringUtils::to_wide("X-API-Key: key\r\nAccept: */*"), L"X-API-Key: key\r\nAccept: */*");
}

TEST(StringUtilsTests, ConvertsWideStringToUtf8)
{
	EXPECT_EQ(StringUtils::to_utf8(L"https://localhost:8443/api/v1"), "https://localhost:8443/api/v1");
	EXPECT_EQ(StringUtils::to_utf8(L"фото.png"), "\xd1\x84\xd0\xbe\xd1\x82\xd0\xbe.png");
}

TEST(StringUtilsTests, ConvertsEmptyWideString)
{
	EXPECT_EQ(StringUtils::to_utf8(L""), "");
}

TEST(StringUtilsTests, FormatsSizeBelowKilobyteInBytes)
{
	EXPECT_EQ(StringUtils::format_byte_size(0), "0 B");
	EXPECT_EQ(StringUtils::format_byte_size(1023), "1023 B");
}

TEST(StringUtilsTests, FormatsSizeWithBase1024)
{
	EXPECT_EQ(StringUtils::format_byte_size(1024), "1.0 KB");
	EXPECT_EQ(StringUtils::format_byte_size(1536), "1.5 KB");
	EXPECT_EQ(StringUtils::format_byte_size(1258291), "1.2 MB");
	EXPECT_EQ(StringUtils::format_byte_size(5ull * 1024 * MIB), "5.0 GB");
	EXPECT_EQ(StringUtils::format_byte_size(2048ull * 1024 * MIB), "2048.0 GB");
}

TEST(StringUtilsTests, FormatsImageSizeLimitAsWholeMegabytes)
{
	EXPECT_EQ(StringUtils::format_byte_size(5 * MIB), "5.0 MB");
}