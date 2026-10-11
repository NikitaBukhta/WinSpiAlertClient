#include "EventsTableFormatter.h"

#include <algorithm>
#include <chrono>
#include <format>

#include "utils/StringUtils.h"

#define COLUMN_GAP "  "
#define RULE_CHAR '-'
#define MAX_FILE_NAME_WIDTH 40
#define TRUNCATION_MARK "..."

namespace cli
{

EventsTableFormatter::EventsTableFormatter(const app::SpialertService::EventsListing& listing, const bool pending)
	: _pending(pending), _truncated(listing.truncated), _total(listing.total)
{
	fit_widths(header_row());
	for (const auto& event : listing.items)
	{
		_rows.push_back(make_row(event));
		fit_widths(_rows.back());
		if (!event.image_uploaded)
			++_without_image_count;
	}
}

std::string EventsTableFormatter::format() const
{
	if (_rows.empty())
		return _pending ? "No events without image\n" : "No events\n";

	std::string text = format_row(header_row()) + format_rule();
	for (const Row& row : _rows)
		text += format_row(row);
	return text + format_summary();
}

void EventsTableFormatter::fit_widths(const Row& row)
{
	for (size_t column = 0; column < row.size(); ++column)
		_widths[column] = std::max(_widths[column], row[column].size());
}

std::string EventsTableFormatter::format_row(const Row& row) const
{
	std::string line;
	for (size_t column = 0; column + 1 < row.size(); ++column)
		line += std::format("{:<{}}" COLUMN_GAP, row[column], _widths[column]);
	return line + row.back() + "\n";
}

std::string EventsTableFormatter::format_rule() const
{
	Row rule;
	for (size_t column = 0; column < rule.size(); ++column)
		rule[column] = std::string(_widths[column], RULE_CHAR);
	return format_row(rule);
}

std::string EventsTableFormatter::format_summary() const
{
	std::string summary = _pending
	                          ? std::format("\nWithout image: {}\n", _rows.size())
	                          : std::format("\nTotal: {}, without image: {}\n", _rows.size(), _without_image_count);
	if (_truncated)
		summary += std::format("Only the newest {} of {} events are shown\n", _rows.size(), _total);
	return summary;
}

EventsTableFormatter::Row EventsTableFormatter::header_row()
{
	return {"ID", "CODE", "FILE", "TIMESTAMP (UTC)", "IMAGE"};
}

EventsTableFormatter::Row EventsTableFormatter::make_row(const api::dto::EventObjectDto& event)
{
	return {std::to_string(event.event_id), std::to_string(event.event_code), format_file_name(event.file_name),
	        format_timestamp(event.timestamp), format_image(event)};
}

std::string EventsTableFormatter::format_file_name(const std::string_view file_name)
{
	if (file_name.size() <= MAX_FILE_NAME_WIDTH)
		return std::string(file_name);

	constexpr size_t kept_size = MAX_FILE_NAME_WIDTH - std::string_view(TRUNCATION_MARK).size();
	return std::string(file_name.substr(0, kept_size)) + TRUNCATION_MARK;
}

std::string EventsTableFormatter::format_timestamp(const utils::time_point timestamp)
{
	return std::format("{:%F %T}", std::chrono::floor<std::chrono::seconds>(timestamp));
}

std::string EventsTableFormatter::format_image(const api::dto::EventObjectDto& event)
{
	if (!event.image_uploaded)
		return "no";
	return std::format("yes ({})", utils::StringUtils::format_byte_size(event.image_size));
}

} // namespace cli
