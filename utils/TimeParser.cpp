#include "TimeParser.h"

#include <charconv>
#include <chrono>
#include <format>

namespace utils
{

time_point TimeParser::parse_iso8601(const std::string_view date_str)
{
	if (date_str.length() < ISO8601_NO_MS_LEN)
		return {};

	int year{}, month{}, day{}, hour{}, minute{}, second{}, millisec{};

	const bool fields_parsed = parse_field(date_str, ISO8601_YEAR_OFFSET, ISO8601_YEAR_LEN, year) &&
	                           parse_field(date_str, ISO8601_MONTH_OFFSET, ISO8601_MONTH_LEN, month) &&
	                           parse_field(date_str, ISO8601_DAY_OFFSET, ISO8601_DAY_LEN, day) &&
	                           parse_field(date_str, ISO8601_HOUR_OFFSET, ISO8601_HOUR_LEN, hour) &&
	                           parse_field(date_str, ISO8601_MIN_OFFSET, ISO8601_MIN_LEN, minute) &&
	                           parse_field(date_str, ISO8601_SEC_OFFSET, ISO8601_SEC_LEN, second);
	if (!fields_parsed)
		return {};

	const bool has_ms =
		date_str.length() >= ISO8601_LEN && date_str[ISO8601_MS_SEPARATOR_OFFSET] == ISO8601_MS_SEPARATOR;
	if (has_ms && !parse_field(date_str, ISO8601_MS_OFFSET, ISO8601_MS_LEN, millisec))
		return {};

	const std::chrono::year_month_day date{std::chrono::year{year} / month / day};
	const bool time_valid = hour <= MAX_HOUR && minute <= MAX_MINUTE && second <= MAX_SECOND;
	if (!date.ok() || !time_valid)
		return {};

	const auto time_duration = std::chrono::hours{hour} + std::chrono::minutes{minute} + std::chrono::seconds{second} +
	                           std::chrono::milliseconds{millisec};

	return std::chrono::sys_days{date} + time_duration;
}

std::string TimeParser::format_iso8601(const time_point time)
{
	return std::format("{:%FT%T}Z", std::chrono::floor<std::chrono::milliseconds>(time));
}

bool TimeParser::parse_field(std::string_view str, size_t offset, size_t length, int& value)
{
	const char* begin = str.data() + offset;
	const char* end = begin + length;
	const auto [parsed_end, error] = std::from_chars(begin, end, value);

	return error == std::errc{} && parsed_end == end && *begin != '-';
}

} // namespace utils
