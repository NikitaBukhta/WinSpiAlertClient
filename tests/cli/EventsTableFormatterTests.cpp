#include <gtest/gtest.h>

#include <string>

#include "api/dto/EventObjectDto.h"
#include "app/SpialertService.h"
#include "cli/EventsTableFormatter.h"
#include "utils/TimeParser.h"

#define FILE_NAME_WIDTH 40

using app::SpialertService;
using cli::EventsTableFormatter;

namespace
{

// image_size 0 means the image is not uploaded
api::dto::EventObjectDto make_event(int event_id, int event_code, const std::string& file_name,
                                    const std::string& timestamp, size_t image_size)
{
	api::dto::EventObjectDto event;
	event.event_id = event_id;
	event.event_code = event_code;
	event.file_name = file_name;
	event.timestamp = utils::TimeParser::parse_iso8601(timestamp);
	event.image_uploaded = image_size > 0;
	event.image_size = image_size;
	return event;
}

SpialertService::EventsListing two_events()
{
	SpialertService::EventsListing listing;
	listing.items = {
		make_event(12, 3, "capture_0002.jpg", "2026-10-11T10:05:07.250Z", 0),
		make_event(9, 10, "capture_0001.png", "2026-10-11T09:59:00.000Z", 1258291),
	};
	listing.total = 2;
	return listing;
}

std::string format_file_name(const std::string& file_name)
{
	SpialertService::EventsListing listing;
	listing.items = {make_event(1, 1, file_name, "2026-10-11T10:00:00.000Z", 0)};
	listing.total = 1;
	return EventsTableFormatter(listing, false).format();
}

} // namespace

TEST(EventsTableFormatterTests, FormatsEveryEventAndCountsThoseWithoutImage)
{
	const std::string expected = "ID  CODE  FILE              TIMESTAMP (UTC)      IMAGE\n"
								 "--  ----  ----------------  -------------------  ------------\n"
								 "12  3     capture_0002.jpg  2026-10-11 10:05:07  no\n"
								 "9   10    capture_0001.png  2026-10-11 09:59:00  yes (1.2 MB)\n"
								 "\n"
								 "Total: 2, without image: 1\n";

	EXPECT_EQ(EventsTableFormatter(two_events(), false).format(), expected);
}

TEST(EventsTableFormatterTests, PendingSummaryCountsShownEvents)
{
	SpialertService::EventsListing listing;
	listing.items = {make_event(12, 3, "capture_0002.jpg", "2026-10-11T10:05:07.000Z", 0)};
	listing.total = 1;

	const std::string table = EventsTableFormatter(listing, true).format();

	EXPECT_TRUE(table.ends_with("\nWithout image: 1\n")) << table;
}

TEST(EventsTableFormatterTests, EmptyListingSaysThereAreNoEvents)
{
	EXPECT_EQ(EventsTableFormatter({}, false).format(), "No events\n");
	EXPECT_EQ(EventsTableFormatter({}, true).format(), "No events without image\n");
}

TEST(EventsTableFormatterTests, TruncatedListingSaysNotEveryEventIsShown)
{
	SpialertService::EventsListing listing = two_events();
	listing.total = 5000;
	listing.truncated = true;

	const std::string table = EventsTableFormatter(listing, false).format();

	EXPECT_TRUE(table.ends_with("\nTotal: 2, without image: 1\nOnly the newest 2 of 5000 events are shown\n")) << table;
}

TEST(EventsTableFormatterTests, LongFileNameIsCutWithAsciiMark)
{
	const std::string table = format_file_name(std::string(96, 'a') + ".png");

	EXPECT_TRUE(table.contains(std::string(FILE_NAME_WIDTH - 3, 'a') + "...  2026-10-11")) << table;
}

TEST(EventsTableFormatterTests, FileNameOfMaxWidthIsKept)
{
	const std::string file_name = std::string(FILE_NAME_WIDTH - 4, 'b') + ".png";

	const std::string table = format_file_name(file_name);

	EXPECT_TRUE(table.contains(file_name + "  2026-10-11")) << table;
}
