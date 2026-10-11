#include <gtest/gtest.h>

#include <string>

#include "api/dto/StatusDto.h"
#include "tests/helpers/DtoParser.h"

using namespace std::chrono;
using api::dto::StatusMapper;
using test_helpers::DtoParser;

TEST(StatusDtoTests, ParsesOnlineStatus)
{
	const auto dto =
		DtoParser::parse<StatusMapper>(R"({"status":"online","api_version":"1.0",)"
		                               R"("server_time":"2026-10-11T12:30:45.123Z","candidate":"Mock Candidate"})");

	EXPECT_TRUE(dto.status);
	EXPECT_EQ(dto.api_version, "1.0");
	EXPECT_EQ(dto.candidate, "Mock Candidate");
	EXPECT_EQ(dto.server_time, sys_days{2026y / October / 11} + 12h + 30min + 45s + 123ms);
}

TEST(StatusDtoTests, NotOnlineStatusIsFalse)
{
	EXPECT_FALSE(DtoParser::parse<StatusMapper>(R"({"status":"maintenance"})").status);
	EXPECT_FALSE(DtoParser::parse<StatusMapper>(R"({"status":"Online"})").status);
	EXPECT_FALSE(DtoParser::parse<StatusMapper>(R"({"status":""})").status);
}

TEST(StatusDtoTests, FieldOrderDoesNotMatter)
{
	const auto dto = DtoParser::parse<StatusMapper>(R"({"candidate":"A","api_version":"2.1","status":"online"})");

	EXPECT_TRUE(dto.status);
	EXPECT_EQ(dto.api_version, "2.1");
	EXPECT_EQ(dto.candidate, "A");
}

TEST(StatusDtoTests, IgnoresUnknownFields)
{
	const auto dto = DtoParser::parse<StatusMapper>(
		R"({"extra":"x","count":5,"flags":[1,2],"nested":{"a":true},"status":"online"})");

	EXPECT_TRUE(dto.status);
}

TEST(StatusDtoTests, MissingFieldsStayDefault)
{
	const auto dto = DtoParser::parse<StatusMapper>("{}");

	EXPECT_FALSE(dto.status);
	EXPECT_TRUE(dto.api_version.empty());
	EXPECT_TRUE(dto.candidate.empty());
	EXPECT_EQ(dto.server_time, utils::time_point{});
}

TEST(StatusDtoTests, EmptyBodyGivesDefaultDto)
{
	const auto dto = DtoParser::parse<StatusMapper>("");

	EXPECT_FALSE(dto.status);
	EXPECT_TRUE(dto.api_version.empty());
}

TEST(StatusDtoTests, ParsesEscapedString)
{
	const auto dto = DtoParser::parse<StatusMapper>(R"({"candidate":"Jane \"JD\" Doe"})");

	EXPECT_EQ(dto.candidate, R"(Jane "JD" Doe)");
}
