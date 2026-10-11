#include <gtest/gtest.h>

#include <string>

#include "api/dto/EventObjectDto.h"
#include "api/dto/EventRequestDto.h"
#include "tests/helpers/DtoParser.h"

using namespace std::chrono;
using api::dto::EventObjectMapper;
using api::dto::EventRequestDto;
using api::dto::EventRequestMapper;
using test_helpers::DtoParser;

TEST(EventRequestDtoTests, WritesTheFourAllowedFields)
{
	EventRequestDto dto;
	dto.event_uid = "3f1c2b9e-6a8d-4f3e-9b7a-1d2e3f4a5b6c";
	dto.event_code = 7;
	dto.file_name = "capture_0001.jpg";
	dto.timestamp = sys_days{2026y / October / 5} + 12h + 34min + 56s;

	EXPECT_EQ(EventRequestMapper::toJson(dto),
	          R"({"event_uid":"3f1c2b9e-6a8d-4f3e-9b7a-1d2e3f4a5b6c","event_code":7,)"
	          R"("file_name":"capture_0001.jpg","timestamp":"2026-10-05T12:34:56.000Z"})");
}

TEST(EventRequestDtoTests, EscapesStrings)
{
	EventRequestDto dto;
	dto.event_uid = R"(uid"with\quote)";

	EXPECT_NE(EventRequestMapper::toJson(dto).find(R"("event_uid":"uid\"with\\quote")"), std::string::npos);
}

TEST(EventRequestDtoTests, OutputParsesBackAsEvent)
{
	EventRequestDto dto;
	dto.event_uid = "uid-0001";
	dto.event_code = 3;
	dto.file_name = "a.png";
	dto.timestamp = sys_days{2026y / October / 11} + 12h + 30min + 45s + 123ms;

	const auto parsed = DtoParser::parse<EventObjectMapper>(EventRequestMapper::toJson(dto));

	EXPECT_EQ(parsed.event_uid, dto.event_uid);
	EXPECT_EQ(parsed.event_code, dto.event_code);
	EXPECT_EQ(parsed.file_name, dto.file_name);
	EXPECT_EQ(parsed.timestamp, dto.timestamp);
}
