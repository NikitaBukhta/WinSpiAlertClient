#include <gtest/gtest.h>

#include <string>

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
