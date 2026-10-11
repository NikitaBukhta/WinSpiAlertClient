#include <gtest/gtest.h>

#include <string>

#include "api/dto/ServerErrorDto.h"
#include "tests/helpers/DtoParser.h"

using api::dto::ServerErrorMapper;
using test_helpers::DtoParser;

TEST(ServerErrorDtoTests, ParsesErrorWithoutDetails)
{
	const auto dto = DtoParser::parse<ServerErrorMapper>(
		R"({"error":{"code":"invalid_api_key","message":"API key is not valid."}})");

	EXPECT_EQ(dto.code, "invalid_api_key");
	EXPECT_EQ(dto.message, "API key is not valid.");
	EXPECT_TRUE(dto.details.empty());
}

TEST(ServerErrorDtoTests, ParsesValidationErrorDetails)
{
	const auto dto = DtoParser::parse<ServerErrorMapper>(
		R"({"error":{"code":"validation_error","message":"Request validation failed",)"
		R"("details":[{"field":"event_code","message":"Input should be less than or equal to 10"}]}})");

	EXPECT_EQ(dto.code, "validation_error");
	EXPECT_EQ(dto.message, "Request validation failed");
	ASSERT_EQ(dto.details.size(), 1u);
	EXPECT_EQ(dto.details[0].field, "event_code");
	EXPECT_EQ(dto.details[0].message, "Input should be less than or equal to 10");
}

TEST(ServerErrorDtoTests, DetailMessagesDoNotReplaceErrorMessage)
{
	const auto dto = DtoParser::parse<ServerErrorMapper>(
		R"({"error":{"details":[{"message":"first","field":"event_uid"},{"field":"file_name","message":"second"}],)"
		R"("code":"validation_error","message":"main",)"
		R"("details_after":[{"message":"ignored"}]}})");

	EXPECT_EQ(dto.code, "validation_error");
	EXPECT_EQ(dto.message, "main");
	ASSERT_EQ(dto.details.size(), 2u);
	EXPECT_EQ(dto.details[0].field, "event_uid");
	EXPECT_EQ(dto.details[0].message, "first");
	EXPECT_EQ(dto.details[1].field, "file_name");
	EXPECT_EQ(dto.details[1].message, "second");
}

TEST(ServerErrorDtoTests, IgnoresFieldsOutsideError)
{
	const auto dto = DtoParser::parse<ServerErrorMapper>(
		R"({"code":"outer","message":"outer","meta":{"code":"nested","message":"nested"},)"
		R"("error":{"code":"not_found","message":"Path not found","extra":{"message":"deeper"}}})");

	EXPECT_EQ(dto.code, "not_found");
	EXPECT_EQ(dto.message, "Path not found");
	EXPECT_TRUE(dto.details.empty());
}

TEST(ServerErrorDtoTests, ErrorAsStringIsEmpty)
{
	const auto dto = DtoParser::parse<ServerErrorMapper>(R"({"error":"invalid JSON"})");

	EXPECT_TRUE(dto.code.empty());
	EXPECT_TRUE(dto.message.empty());
}

TEST(ServerErrorDtoTests, HtmlBodyIsNotJson)
{
	EXPECT_EQ(DtoParser::try_parse<ServerErrorMapper>("<html><body><h1>502 Bad Gateway</h1></body></html>"),
	          std::nullopt);
}

TEST(ServerErrorDtoTests, EmptyBodyIsNotJson)
{
	EXPECT_EQ(DtoParser::try_parse<ServerErrorMapper>(""), std::nullopt);
}
