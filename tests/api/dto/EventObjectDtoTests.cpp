#include <gtest/gtest.h>

#include <string>

#include "api/dto/EventObjectDto.h"
#include "tests/helpers/DtoParser.h"

using namespace std::chrono;
using api::dto::EventObjectDto;
using api::dto::EventObjectMapper;
using test_helpers::DtoParser;

TEST(EventObjectDtoTests, ParsesEventWithUploadedImage)
{
	const auto dto = DtoParser::parse<EventObjectMapper>(
		R"({"event_id":42,"event_uid":"uid-0001","event_code":3,"file_name":"a.png",)"
		R"("timestamp":"2026-10-11T12:30:45.000Z","received_at":"2026-10-11T12:30:46.250Z",)"
		R"("image_uploaded":true,"image_size":108,)"
		R"("image_sha256":"a4faa983b5f97e29bff000de388c2b0d0baf9c67cc9d0ad1183a114a6a1bed54",)"
		R"("image_uploaded_at":"2026-10-11T12:31:00.500Z"})");

	EXPECT_EQ(dto.event_id, 42);
	EXPECT_EQ(dto.event_uid, "uid-0001");
	EXPECT_EQ(dto.event_code, 3);
	EXPECT_EQ(dto.file_name, "a.png");
	EXPECT_EQ(dto.timestamp, sys_days{2026y / October / 11} + 12h + 30min + 45s);
	EXPECT_EQ(dto.received_at, sys_days{2026y / October / 11} + 12h + 30min + 46s + 250ms);
	EXPECT_TRUE(dto.image_uploaded);
	EXPECT_EQ(dto.image_size, 108u);
	EXPECT_EQ(dto.image_sha256, "a4faa983b5f97e29bff000de388c2b0d0baf9c67cc9d0ad1183a114a6a1bed54");
	EXPECT_EQ(dto.image_uploaded_at, sys_days{2026y / October / 11} + 12h + 31min + 500ms);
}

TEST(EventObjectDtoTests, NullImageFieldsStayDefault)
{
	const auto dto = DtoParser::parse<EventObjectMapper>(
		R"({"event_id":1,"image_uploaded":false,"image_size":null,"image_sha256":null,)"
		R"("image_uploaded_at":null})");

	EXPECT_EQ(dto.event_id, 1);
	EXPECT_FALSE(dto.image_uploaded);
	EXPECT_EQ(dto.image_size, 0u);
	EXPECT_TRUE(dto.image_sha256.empty());
	EXPECT_EQ(dto.image_uploaded_at, utils::time_point{});
}

TEST(EventObjectDtoTests, NonNegativeIntegersComeThroughUint)
{
	const auto dto = DtoParser::parse<EventObjectMapper>(R"({"event_id":7,"event_code":10})");

	EXPECT_EQ(dto.event_id, 7);
	EXPECT_EQ(dto.event_code, 10);
}

TEST(EventObjectDtoTests, NegativeIntegersComeThroughInt)
{
	const auto dto = DtoParser::parse<EventObjectMapper>(R"({"event_id":-1,"event_code":-5})");

	EXPECT_EQ(dto.event_id, -1);
	EXPECT_EQ(dto.event_code, -5);
}

TEST(EventObjectDtoTests, ImageSizeAboveFourGigabytesComesThroughUint64)
{
	const auto dto = DtoParser::parse<EventObjectMapper>(R"({"image_size":5000000000})");

	EXPECT_EQ(dto.image_size, size_t{5'000'000'000});
}

TEST(EventObjectDtoTests, ZeroImageSize)
{
	EXPECT_EQ(DtoParser::parse<EventObjectMapper>(R"({"image_size":0})").image_size, 0u);
}

TEST(EventObjectDtoTests, FieldOrderDoesNotMatter)
{
	const auto dto = DtoParser::parse<EventObjectMapper>(
		R"({"image_size":5,"file_name":"b.jpg","event_code":2,"event_id":9,"event_uid":"uid-0009"})");

	EXPECT_EQ(dto.event_id, 9);
	EXPECT_EQ(dto.event_uid, "uid-0009");
	EXPECT_EQ(dto.event_code, 2);
	EXPECT_EQ(dto.file_name, "b.jpg");
	EXPECT_EQ(dto.image_size, 5u);
}

TEST(EventObjectDtoTests, IgnoresUnknownFields)
{
	const auto dto =
		DtoParser::parse<EventObjectMapper>(R"({"extra":"x","ratio":1.5,"flags":[1,2],"event_id":3,"note":null})");

	EXPECT_EQ(dto.event_id, 3);
}

TEST(EventObjectDtoTests, MissingFieldsStayDefault)
{
	const auto dto = DtoParser::parse<EventObjectMapper>("{}");

	EXPECT_EQ(dto.event_id, 0);
	EXPECT_EQ(dto.event_code, 0);
	EXPECT_TRUE(dto.event_uid.empty());
	EXPECT_TRUE(dto.file_name.empty());
	EXPECT_EQ(dto.timestamp, utils::time_point{});
	EXPECT_EQ(dto.received_at, utils::time_point{});
	EXPECT_FALSE(dto.image_uploaded);
	EXPECT_EQ(dto.image_size, 0u);
}

TEST(EventObjectDtoTests, EmptyBodyIsNotJson)
{
	EXPECT_EQ(DtoParser::try_parse<EventObjectMapper>(""), std::nullopt);
}

TEST(EventObjectDtoTests, InvalidJsonIsNotAnEvent)
{
	EXPECT_EQ(DtoParser::try_parse<EventObjectMapper>(R"({"event_id":42,"file_name":)"), std::nullopt);
	EXPECT_EQ(DtoParser::try_parse<EventObjectMapper>("<html>Bad Gateway</html>"), std::nullopt);
}
