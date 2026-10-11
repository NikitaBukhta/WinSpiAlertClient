#include <gtest/gtest.h>

#include "tests/helpers/TestConstants.h"
#include "utils/UrlParser.h"

using utils::UrlParser;

TEST(UrlParserTests, ParsesUrlWithoutPort)
{
	const auto url = UrlParser::parse("https://challenge.spialert.com/api/v1");

	ASSERT_TRUE(url.has_value());
	EXPECT_EQ(url->scheme, "https");
	EXPECT_EQ(url->host, "challenge.spialert.com");
	EXPECT_EQ(url->port, HTTPS_DEFAULT_PORT);
	EXPECT_EQ(url->path, "/api/v1");
	EXPECT_TRUE(url->query_and_fragment.empty());
	EXPECT_FALSE(url->has_user_info);
}

TEST(UrlParserTests, ParsesUrlWithPort)
{
	const auto url = UrlParser::parse("https://127.0.0.1:8443/api/v1");

	ASSERT_TRUE(url.has_value());
	EXPECT_EQ(url->host, "127.0.0.1");
	EXPECT_EQ(url->port, 8443);
	EXPECT_EQ(url->path, "/api/v1");
}

TEST(UrlParserTests, HttpGetsItsDefaultPort)
{
	const auto url = UrlParser::parse("http://localhost/api");

	ASSERT_TRUE(url.has_value());
	EXPECT_EQ(url->scheme, "http");
	EXPECT_EQ(url->port, HTTP_DEFAULT_PORT);
}

TEST(UrlParserTests, SchemeIsLowercased)
{
	const auto url = UrlParser::parse("HTTPS://localhost/api");

	ASSERT_TRUE(url.has_value());
	EXPECT_EQ(url->scheme, "https");
}

TEST(UrlParserTests, KeepsTrailingSlash)
{
	const auto url = UrlParser::parse("https://localhost/api/v1/");

	ASSERT_TRUE(url.has_value());
	EXPECT_EQ(url->path, "/api/v1/");
}

TEST(UrlParserTests, UrlWithoutPathHasEmptyPath)
{
	const auto url = UrlParser::parse("https://localhost");

	ASSERT_TRUE(url.has_value());
	EXPECT_EQ(url->host, "localhost");
	EXPECT_TRUE(url->path.empty());
}

TEST(UrlParserTests, SeparatesQueryAndFragment)
{
	const auto url = UrlParser::parse("https://localhost/api?debug=1#top");

	ASSERT_TRUE(url.has_value());
	EXPECT_EQ(url->path, "/api");
	EXPECT_EQ(url->query_and_fragment, "?debug=1#top");
}

TEST(UrlParserTests, DetectsUserInfo)
{
	const auto with_password = UrlParser::parse("https://user:secret@localhost/api");
	const auto with_user = UrlParser::parse("https://user@localhost/api");

	ASSERT_TRUE(with_password.has_value());
	ASSERT_TRUE(with_user.has_value());
	EXPECT_TRUE(with_password->has_user_info);
	EXPECT_TRUE(with_user->has_user_info);
	EXPECT_EQ(with_password->host, "localhost");
}

TEST(UrlParserTests, RejectsMalformedUrls)
{
	EXPECT_FALSE(UrlParser::parse("").has_value());
	EXPECT_FALSE(UrlParser::parse("not a url").has_value());
	EXPECT_FALSE(UrlParser::parse("challenge.spialert.com/api/v1").has_value());
	EXPECT_FALSE(UrlParser::parse("https://").has_value());
	EXPECT_FALSE(UrlParser::parse("https://localhost:99999/api").has_value());
	EXPECT_FALSE(UrlParser::parse("https://localhost:port/api").has_value());
}

TEST(UrlParserTests, RejectsUnsupportedSchemes)
{
	EXPECT_FALSE(UrlParser::parse("ftp://localhost/api").has_value());
	EXPECT_FALSE(UrlParser::parse("file:///C:/config.json").has_value());
}

TEST(UrlParserTests, RejectsNonAsciiUrls)
{
	EXPECT_FALSE(UrlParser::parse("https://пример.рф/api").has_value());
}
