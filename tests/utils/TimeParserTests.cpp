#include <gtest/gtest.h>

#include "utils/TimeParser.h"

using namespace std::chrono;
using utils::TimeParser;

TEST(TimeParserTests, ParsesFullTimestampWithMilliseconds)
{
	constexpr auto expected = sys_days{2026y / October / 11} + 12h + 30min + 45s + 123ms;

	EXPECT_EQ(TimeParser::parse_iso8601("2026-10-11T12:30:45.123Z"), expected);
}

TEST(TimeParserTests, ParsesUnixEpoch)
{
	EXPECT_EQ(TimeParser::parse_iso8601("1970-01-01T00:00:00.000Z"), utils::time_point{});
}

TEST(TimeParserTests, ParsesLeapDay)
{
	constexpr auto expected = sys_days{2024y / February / 29} + 23h + 59min + 59s + 999ms;

	EXPECT_EQ(TimeParser::parse_iso8601("2024-02-29T23:59:59.999Z"), expected);
}

TEST(TimeParserTests, EmptyStringReturnsDefault)
{
	EXPECT_EQ(TimeParser::parse_iso8601(""), utils::time_point{});
}

TEST(TimeParserTests, ShortStringReturnsDefault)
{
	EXPECT_EQ(TimeParser::parse_iso8601("2026-10-11"), utils::time_point{});
	EXPECT_EQ(TimeParser::parse_iso8601("2026-10-11T12:30:45"), utils::time_point{});
}

TEST(TimeParserTests, ParsesTimestampWithoutMilliseconds)
{
	constexpr auto expected = sys_days{2026y / October / 11} + 12h + 30min + 45s;

	EXPECT_EQ(TimeParser::parse_iso8601("2026-10-11T12:30:45Z"), expected);
}

TEST(TimeParserTests, StringViewIsNotReadPastItsEnd)
{
	// the view ends before ".123Z": milliseconds outside the view must be ignored
	constexpr std::string_view without_ms = std::string_view("2026-10-11T12:30:45Z.123Z").substr(0, 20);
	constexpr auto expected = sys_days{2026y / October / 11} + 12h + 30min + 45s;

	EXPECT_EQ(TimeParser::parse_iso8601(without_ms), expected);
}

TEST(TimeParserTests, NonDigitFieldReturnsDefault)
{
	EXPECT_EQ(TimeParser::parse_iso8601("2026-1x-11T12:30:45.123Z"), utils::time_point{});
	EXPECT_EQ(TimeParser::parse_iso8601("2026-10-11T-1:30:45.123Z"), utils::time_point{});
	EXPECT_EQ(TimeParser::parse_iso8601("2026-10-11T12:30:45.1x3Z"), utils::time_point{});
	EXPECT_EQ(TimeParser::parse_iso8601("not a timestamp at all!!"), utils::time_point{});
}

TEST(TimeParserTests, InvalidDateReturnsDefault)
{
	EXPECT_EQ(TimeParser::parse_iso8601("2026-02-30T12:30:45.123Z"), utils::time_point{});
	EXPECT_EQ(TimeParser::parse_iso8601("2026-13-01T12:30:45.123Z"), utils::time_point{});
	EXPECT_EQ(TimeParser::parse_iso8601("2025-02-29T12:30:45.123Z"), utils::time_point{});
}

TEST(TimeParserTests, InvalidTimeReturnsDefault)
{
	EXPECT_EQ(TimeParser::parse_iso8601("2026-10-11T24:00:00.000Z"), utils::time_point{});
	EXPECT_EQ(TimeParser::parse_iso8601("2026-10-11T12:60:00.000Z"), utils::time_point{});
	EXPECT_EQ(TimeParser::parse_iso8601("2026-10-11T12:30:60.000Z"), utils::time_point{});
}

TEST(TimeParserTests, FormatsWithMillisecondsAndZ)
{
	constexpr auto time = sys_days{2026y / October / 11} + 12h + 30min + 45s + 123ms;

	EXPECT_EQ(TimeParser::format_iso8601(time), "2026-10-11T12:30:45.123Z");
}

TEST(TimeParserTests, FormatsWholeSecondsWithZeroMilliseconds)
{
	EXPECT_EQ(TimeParser::format_iso8601(sys_days{2026y / January / 2} + 3h), "2026-01-02T03:00:00.000Z");
}

TEST(TimeParserTests, FormatTruncatesBelowMilliseconds)
{
	constexpr auto time = sys_days{2026y / October / 11} + 999ms + 999us;

	EXPECT_EQ(TimeParser::format_iso8601(time), "2026-10-11T00:00:00.999Z");
}

TEST(TimeParserTests, FormattedTimeParsesBack)
{
	const auto now = floor<milliseconds>(system_clock::now());

	EXPECT_EQ(TimeParser::parse_iso8601(TimeParser::format_iso8601(now)), now);
}
