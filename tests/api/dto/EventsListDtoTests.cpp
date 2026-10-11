#include <gtest/gtest.h>

#include <string>

#include "api/dto/EventsListDto.h"
#include "tests/helpers/DtoParser.h"

using namespace std::chrono;
using api::dto::EventsListDto;
using api::dto::EventsListMapper;
using test_helpers::DtoParser;

namespace
{

std::string event_json(int event_id)
{
	return R"({"event_id":)" + std::to_string(event_id) + R"(,"event_uid":"uid-)" + std::to_string(event_id) +
	       R"(","event_code":3,"file_name":"a.png","image_uploaded":false,"image_size":null})";
}

} // namespace

TEST(EventsListDtoTests, ParsesPageWithItems)
{
	const auto dto = DtoParser::parse<EventsListMapper>(
		R"({"total":2,"limit":20,"offset":0,"items":[)"
		R"({"event_id":2,"event_uid":"uid-0002","event_code":5,"file_name":"b.jpg",)"
		R"("timestamp":"2026-10-11T12:30:45.000Z","image_uploaded":true,"image_size":103},)"
		R"({"event_id":1,"event_uid":"uid-0001","event_code":3,"file_name":"a.png",)"
		R"("image_uploaded":false,"image_size":null}]})");

	EXPECT_EQ(dto.total, 2);
	EXPECT_EQ(dto.limit, 20);
	EXPECT_EQ(dto.offset, 0);
	ASSERT_EQ(dto.items.size(), 2u);

	EXPECT_EQ(dto.items[0].event_id, 2);
	EXPECT_EQ(dto.items[0].event_uid, "uid-0002");
	EXPECT_EQ(dto.items[0].event_code, 5);
	EXPECT_EQ(dto.items[0].file_name, "b.jpg");
	EXPECT_EQ(dto.items[0].timestamp, sys_days{2026y / October / 11} + 12h + 30min + 45s);
	EXPECT_TRUE(dto.items[0].image_uploaded);
	EXPECT_EQ(dto.items[0].image_size, 103u);

	EXPECT_EQ(dto.items[1].event_id, 1);
	EXPECT_EQ(dto.items[1].event_uid, "uid-0001");
	EXPECT_FALSE(dto.items[1].image_uploaded);
	EXPECT_EQ(dto.items[1].image_size, 0u);
}

TEST(EventsListDtoTests, ParsesEmptyPage)
{
	const auto dto = DtoParser::parse<EventsListMapper>(R"({"total":0,"limit":20,"offset":40,"items":[]})");

	EXPECT_EQ(dto.total, 0);
	EXPECT_EQ(dto.limit, 20);
	EXPECT_EQ(dto.offset, 40);
	EXPECT_TRUE(dto.items.empty());
}

TEST(EventsListDtoTests, TopLevelFieldsAfterItemsAreParsed)
{
	const auto dto =
		DtoParser::parse<EventsListMapper>(R"({"items":[)" + event_json(7) + R"(],"total":15,"limit":1,"offset":3})");

	EXPECT_EQ(dto.total, 15);
	EXPECT_EQ(dto.limit, 1);
	EXPECT_EQ(dto.offset, 3);
	ASSERT_EQ(dto.items.size(), 1u);
	EXPECT_EQ(dto.items[0].event_id, 7);
}

TEST(EventsListDtoTests, ItemFieldsDoNotLeakToTopLevel)
{
	// "total" / "limit" inside an item belong to the item (and are ignored there), not to the page
	const auto dto = DtoParser::parse<EventsListMapper>(
		R"({"total":1,"limit":20,"offset":0,"items":[{"event_id":5,"total":99,"limit":99}]})");

	EXPECT_EQ(dto.total, 1);
	EXPECT_EQ(dto.limit, 20);
	ASSERT_EQ(dto.items.size(), 1u);
	EXPECT_EQ(dto.items[0].event_id, 5);
}

TEST(EventsListDtoTests, TopLevelFieldsDoNotLeakToItems)
{
	const auto dto = DtoParser::parse<EventsListMapper>(R"({"event_id":77,"items":[{"event_code":2}]})");

	ASSERT_EQ(dto.items.size(), 1u);
	EXPECT_EQ(dto.items[0].event_id, 0);
	EXPECT_EQ(dto.items[0].event_code, 2);
}

TEST(EventsListDtoTests, ParsesFullPageOfItems)
{
	// many items force vector reallocations: every item handler must still write into its own element
	constexpr int item_count = 100;
	std::string json = R"({"total":100,"limit":100,"offset":0,"items":[)";
	for (int id = item_count; id >= 1; --id)
		json += event_json(id) + (id > 1 ? "," : "");
	json += "]}";

	const auto dto = DtoParser::parse<EventsListMapper>(json);

	ASSERT_EQ(dto.items.size(), static_cast<size_t>(item_count));
	for (int i = 0; i < item_count; ++i)
	{
		EXPECT_EQ(dto.items[i].event_id, item_count - i) << "item " << i;
		EXPECT_EQ(dto.items[i].event_uid, "uid-" + std::to_string(item_count - i)) << "item " << i;
	}
}

TEST(EventsListDtoTests, NestedValuesInsideItemKeepItemBoundaries)
{
	const auto dto = DtoParser::parse<EventsListMapper>(
		R"({"items":[)"
		R"({"event_id":1,"meta":{"source":"x","depth":2},"tags":[1,2,{"a":3}],"event_code":4},)"
		R"({"event_id":2}],"total":2})");

	ASSERT_EQ(dto.items.size(), 2u);
	EXPECT_EQ(dto.items[0].event_code, 4);
	EXPECT_EQ(dto.items[1].event_id, 2);
	EXPECT_EQ(dto.total, 2);
}

TEST(EventsListDtoTests, IgnoresUnknownTopLevelFields)
{
	const auto dto =
		DtoParser::parse<EventsListMapper>(R"({"next":null,"warnings":[],"ratio":0.5,"total":3,"items":[]})");

	EXPECT_EQ(dto.total, 3);
	EXPECT_TRUE(dto.items.empty());
}

TEST(EventsListDtoTests, ItemReceivesValuesOfEveryType)
{
	const auto dto = DtoParser::parse<EventsListMapper>(
		R"({"items":[{"event_code":-2,"image_size":5000000000,"image_uploaded":true,"file_name":"a.png"}]})");

	ASSERT_EQ(dto.items.size(), 1u);
	EXPECT_EQ(dto.items[0].event_code, -2);
	EXPECT_EQ(dto.items[0].image_size, size_t{5'000'000'000});
	EXPECT_TRUE(dto.items[0].image_uploaded);
	EXPECT_EQ(dto.items[0].file_name, "a.png");
}

TEST(EventsListDtoTests, IgnoresTopLevelValuesOfEveryType)
{
	const auto dto = DtoParser::parse<EventsListMapper>(
		R"({"has_more":false,"cursor":"abc","big":5000000000,"total":1,"items":[{"event_id":1}]})");

	EXPECT_EQ(dto.total, 1);
	ASSERT_EQ(dto.items.size(), 1u);
	EXPECT_EQ(dto.items[0].event_id, 1);
}

TEST(EventsListDtoTests, EmptyBodyGivesDefaultDto)
{
	const auto dto = DtoParser::parse<EventsListMapper>("");

	EXPECT_EQ(dto.total, 0);
	EXPECT_TRUE(dto.items.empty());
}
