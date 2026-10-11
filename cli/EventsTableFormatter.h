#ifndef WINSPIALERTCLIENT_EVENTSTABLEFORMATTER_H
#define WINSPIALERTCLIENT_EVENTSTABLEFORMATTER_H

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "app/SpialertService.h"

#define EVENTS_TABLE_COLUMNS 5

namespace cli
{

class EventsTableFormatter
{
public:
	EventsTableFormatter(const app::SpialertService::EventsListing& listing, bool pending);

	[[nodiscard]] std::string format() const;

private:
	using Row = std::array<std::string, EVENTS_TABLE_COLUMNS>;

	void fit_widths(const Row& row);
	[[nodiscard]] std::string format_row(const Row& row) const;
	[[nodiscard]] std::string format_rule() const;
	[[nodiscard]] std::string format_summary() const;
	static Row header_row();
	static Row make_row(const api::dto::EventObjectDto& event);
	static std::string format_file_name(std::string_view file_name);
	static std::string format_timestamp(utils::time_point timestamp);
	static std::string format_image(const api::dto::EventObjectDto& event);

private:
	bool _pending;
	bool _truncated;
	int _total;
	int _without_image_count{};
	std::vector<Row> _rows;
	std::array<size_t, EVENTS_TABLE_COLUMNS> _widths{};
};

} // namespace cli

#endif // WINSPIALERTCLIENT_EVENTSTABLEFORMATTER_H
