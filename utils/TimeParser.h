#ifndef WINSPIALERTCLIENT_TIMEPARSER_H
#define WINSPIALERTCLIENT_TIMEPARSER_H

#include <chrono>
#include <string_view>

// str size for "yyyy-mm-ddThh:mm:ss.000Z"
#define ISO8601_LEN 24
// str size for "yyyy-mm-ddThh:mm:ssZ" (no milliseconds)
#define ISO8601_NO_MS_LEN 20

#define ISO8601_YEAR_OFFSET 0
#define ISO8601_MONTH_OFFSET 5
#define ISO8601_DAY_OFFSET 8
#define ISO8601_HOUR_OFFSET 11
#define ISO8601_MIN_OFFSET 14
#define ISO8601_SEC_OFFSET 17
#define ISO8601_MS_SEPARATOR_OFFSET 19
#define ISO8601_MS_OFFSET 20

#define ISO8601_YEAR_LEN 4
#define ISO8601_MONTH_LEN 2
#define ISO8601_DAY_LEN 2
#define ISO8601_HOUR_LEN 2
#define ISO8601_MIN_LEN 2
#define ISO8601_SEC_LEN 2
#define ISO8601_MS_LEN 3

#define ISO8601_MS_SEPARATOR '.'

#define MAX_HOUR 23
#define MAX_MINUTE 59
#define MAX_SECOND 59

namespace utils
{

using time_point = std::chrono::system_clock::time_point;

class TimeParser
{
public:
	// Accepts "yyyy-mm-ddThh:mm:ss.000Z" and "yyyy-mm-ddThh:mm:ssZ"; returns time_point{} on invalid input
	static time_point parse_iso8601(std::string_view date_str);

private:
	static bool parse_field(std::string_view str, size_t offset, size_t length, int& value);
};
} // namespace utils

#endif // WINSPIALERTCLIENT_TIMEPARSER_H
