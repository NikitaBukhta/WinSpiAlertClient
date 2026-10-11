#include <gtest/gtest.h>

#include <string>

#include "cli/ClientConfigLoader.h"
#include "tests/helpers/TempFile.h"
#include "tests/helpers/TestConstants.h"

using cli::ClientConfigLoader;
using cli::ConfigSource;
using test_helpers::TempFile;

#define VALID_CONFIG R"({"base_url": "https://challenge.spialert.com/api/v1", "api_key": "chk_test"})"

namespace
{

class ClientConfigLoaderTests : public ::testing::Test
{
protected:
	static std::expected<cli::ClientConfig, std::string> parse(const std::string& json, ConfigSource source = {})
	{
		return ClientConfigLoader(std::move(source)).parse(json);
	}

	static std::string base_url_config(const std::string& base_url)
	{
		return R"({"base_url": ")" + base_url + R"(", "api_key": "chk_test"})";
	}
};

} // namespace

TEST_F(ClientConfigLoaderTests, ParsesValidConfig)
{
	const auto config = parse(VALID_CONFIG);

	ASSERT_TRUE(config.has_value()) << config.error();
	EXPECT_EQ(config->host, "challenge.spialert.com");
	EXPECT_EQ(config->port, HTTPS_DEFAULT_PORT);
	EXPECT_EQ(config->base_path, "/api/v1");
	EXPECT_EQ(config->api_key, "chk_test");
}

TEST_F(ClientConfigLoaderTests, ParsesUrlWithPort)
{
	const auto config = parse(base_url_config("https://127.0.0.1:8443/api/v1"));

	ASSERT_TRUE(config.has_value()) << config.error();
	EXPECT_EQ(config->host, "127.0.0.1");
	EXPECT_EQ(config->port, 8443);
	EXPECT_EQ(config->base_path, "/api/v1");
}

TEST_F(ClientConfigLoaderTests, RemovesTrailingSlashFromPath)
{
	const auto with_slash = parse(base_url_config("https://localhost/api/v1/"));
	const auto root = parse(base_url_config("https://localhost/"));
	const auto no_path = parse(base_url_config("https://localhost"));

	ASSERT_TRUE(with_slash.has_value()) << with_slash.error();
	ASSERT_TRUE(root.has_value()) << root.error();
	ASSERT_TRUE(no_path.has_value()) << no_path.error();
	EXPECT_EQ(with_slash->base_path, "/api/v1");
	EXPECT_EQ(root->base_path, "");
	EXPECT_EQ(no_path->base_path, "");
}

TEST_F(ClientConfigLoaderTests, IgnoresUnknownAndNestedFields)
{
	const auto config = parse(R"({"timeout": 5, "proxy": {"base_url": "https://proxy/x", "api_key": "other"},)"
	                          R"("base_url": "https://localhost/api", "api_key": "chk_test"})");

	ASSERT_TRUE(config.has_value()) << config.error();
	EXPECT_EQ(config->host, "localhost");
	EXPECT_EQ(config->api_key, "chk_test");
}

TEST_F(ClientConfigLoaderTests, MissingBaseUrlIsError)
{
	const auto config = parse(R"({"api_key": "chk_test"})");

	ASSERT_FALSE(config.has_value());
	EXPECT_EQ(config.error(), "base_url is not set: add it to secret/config.json or pass --url");
}

TEST_F(ClientConfigLoaderTests, MissingApiKeyIsError)
{
	const auto config = parse(R"({"base_url": "https://localhost/api"})");

	ASSERT_FALSE(config.has_value());
	EXPECT_EQ(config.error(), "api_key is not set: add it to secret/config.json or pass --key");
}

TEST_F(ClientConfigLoaderTests, EmptyOrNonStringFieldsAreMissing)
{
	EXPECT_FALSE(parse(R"({"base_url": "https://localhost/api", "api_key": ""})").has_value());
	EXPECT_FALSE(parse(R"({"base_url": "", "api_key": "chk_test"})").has_value());
	EXPECT_FALSE(parse(R"({"base_url": "https://localhost/api", "api_key": 123})").has_value());
	EXPECT_FALSE(parse(R"({"base_url": null, "api_key": "chk_test"})").has_value());
}

TEST_F(ClientConfigLoaderTests, BrokenJsonIsError)
{
	const auto config = parse(R"({"base_url": "https://localhost/api", "api_key": )");

	ASSERT_FALSE(config.has_value());
	EXPECT_TRUE(config.error().starts_with("config file secret/config.json is not valid JSON:")) << config.error();
}

TEST_F(ClientConfigLoaderTests, EmptyFileIsBrokenJson)
{
	const auto config = parse("");

	ASSERT_FALSE(config.has_value());
	EXPECT_TRUE(config.error().contains("is not valid JSON")) << config.error();
}

TEST_F(ClientConfigLoaderTests, RootThatIsNotObjectHasNoFields)
{
	const auto config = parse(R"(["https://localhost/api", "chk_test"])");

	ASSERT_FALSE(config.has_value());
	EXPECT_TRUE(config.error().starts_with("base_url is not set")) << config.error();
}

TEST_F(ClientConfigLoaderTests, HttpIsError)
{
	const auto config = parse(base_url_config("http://challenge.spialert.com/api/v1"));

	ASSERT_FALSE(config.has_value());
	EXPECT_EQ(config.error(), "base_url must use https: http://challenge.spialert.com/api/v1");
}

TEST_F(ClientConfigLoaderTests, UppercaseHttpsIsAccepted)
{
	EXPECT_TRUE(parse(base_url_config("HTTPS://localhost/api")).has_value());
}

TEST_F(ClientConfigLoaderTests, InvalidUrlIsError)
{
	const auto config = parse(base_url_config("challenge.spialert.com/api/v1"));

	ASSERT_FALSE(config.has_value());
	EXPECT_EQ(config.error(), "base_url is not a valid URL: challenge.spialert.com/api/v1");
}

TEST_F(ClientConfigLoaderTests, QueryOrFragmentIsError)
{
	const auto query = parse(base_url_config("https://localhost/api?v=1"));
	const auto fragment = parse(base_url_config("https://localhost/api#top"));

	ASSERT_FALSE(query.has_value());
	ASSERT_FALSE(fragment.has_value());
	EXPECT_EQ(query.error(), "base_url must not contain a query or fragment: https://localhost/api?v=1");
	EXPECT_TRUE(fragment.error().starts_with("base_url must not contain a query or fragment")) << fragment.error();
}

TEST_F(ClientConfigLoaderTests, UserInfoIsErrorAndIsNotPrinted)
{
	const auto config = parse(base_url_config("http://user:hunter2@localhost/api"));

	ASSERT_FALSE(config.has_value());
	EXPECT_EQ(config.error(), "base_url must not contain a user name or password");
}

TEST_F(ClientConfigLoaderTests, ErrorsNeverContainKey)
{
	const auto http = parse(R"({"base_url": "http://localhost/api", "api_key": "secret-key-1"})");
	const auto invalid = parse(R"({"base_url": "localhost", "api_key": "secret-key-1"})");
	const auto broken = parse(R"({"base_url": "https://localhost/api", "api_key": "secret-key-1")");

	ASSERT_FALSE(http.has_value());
	ASSERT_FALSE(invalid.has_value());
	ASSERT_FALSE(broken.has_value());
	EXPECT_FALSE(http.error().contains("secret-key"));
	EXPECT_FALSE(invalid.error().contains("secret-key"));
	EXPECT_FALSE(broken.error().contains("secret-key"));
}

TEST_F(ClientConfigLoaderTests, UrlFlagReplacesFileValue)
{
	const auto config = parse(VALID_CONFIG, {.url = "https://127.0.0.1:8443/api/v1"});

	ASSERT_TRUE(config.has_value()) << config.error();
	EXPECT_EQ(config->host, "127.0.0.1");
	EXPECT_EQ(config->port, 8443);
	EXPECT_EQ(config->api_key, "chk_test");
}

TEST_F(ClientConfigLoaderTests, KeyFlagReplacesFileValue)
{
	const auto config = parse(VALID_CONFIG, {.key = "test-key"});

	ASSERT_TRUE(config.has_value()) << config.error();
	EXPECT_EQ(config->host, "challenge.spialert.com");
	EXPECT_EQ(config->api_key, "test-key");
}

TEST_F(ClientConfigLoaderTests, FlagFillsFieldMissingInFile)
{
	const auto config = parse(R"({"base_url": "https://localhost/api"})", {.key = "test-key"});

	ASSERT_TRUE(config.has_value()) << config.error();
	EXPECT_EQ(config->api_key, "test-key");
}

TEST_F(ClientConfigLoaderTests, InvalidUrlFlagIsErrorEvenIfFileIsValid)
{
	const auto config = parse(VALID_CONFIG, {.url = "http://localhost/api"});

	ASSERT_FALSE(config.has_value());
	EXPECT_EQ(config.error(), "base_url must use https: http://localhost/api");
}

TEST_F(ClientConfigLoaderTests, LoadsExplicitConfigFile)
{
	const TempFile file("config.json", VALID_CONFIG);

	const auto config = ClientConfigLoader({.config_path = file.path()}).load();

	ASSERT_TRUE(config.has_value()) << config.error();
	EXPECT_EQ(config->host, "challenge.spialert.com");
	EXPECT_EQ(config->api_key, "chk_test");
}

TEST_F(ClientConfigLoaderTests, FlagsReplaceExplicitConfigFileValues)
{
	const TempFile file("config.json", VALID_CONFIG);

	const auto config =
		ClientConfigLoader({.config_path = file.path(), .url = "https://localhost:8443/api", .key = "test-key"}).load();

	ASSERT_TRUE(config.has_value()) << config.error();
	EXPECT_EQ(config->host, "localhost");
	EXPECT_EQ(config->api_key, "test-key");
}

TEST_F(ClientConfigLoaderTests, MissingExplicitConfigFileIsError)
{
	const auto config = ClientConfigLoader({.config_path = "Z:\\no\\such\\config.json"}).load();

	ASSERT_FALSE(config.has_value());
	EXPECT_EQ(config.error(), "config file Z:\\no\\such\\config.json not found");
}

TEST_F(ClientConfigLoaderTests, MissingExplicitConfigFileIsErrorEvenWithBothFlags)
{
	const auto config =
		ClientConfigLoader({.config_path = "Z:\\no\\such\\config.json", .url = "https://localhost/api", .key = "k"})
			.load();

	EXPECT_FALSE(config.has_value());
}

TEST_F(ClientConfigLoaderTests, BrokenConfigFileIsError)
{
	const TempFile file("config.json", "{ not json");

	const auto config = ClientConfigLoader({.config_path = file.path()}).load();

	ASSERT_FALSE(config.has_value());
	EXPECT_TRUE(config.error().contains("is not valid JSON")) << config.error();
}

TEST_F(ClientConfigLoaderTests, BothFlagsWorkWithoutConfigFile)
{
	const auto config = ClientConfigLoader({.url = "https://127.0.0.1:8443/api/v1", .key = "test-key"}).load();

	ASSERT_TRUE(config.has_value()) << config.error();
	EXPECT_EQ(config->host, "127.0.0.1");
	EXPECT_EQ(config->port, 8443);
	EXPECT_EQ(config->base_path, "/api/v1");
	EXPECT_EQ(config->api_key, "test-key");
}

TEST_F(ClientConfigLoaderTests, NumberFieldIsWrongType)
{
	const auto config = parse(R"({"base_url": "https://localhost/api", "api_key": 12345})");

	ASSERT_FALSE(config.has_value());
	EXPECT_EQ(config.error(), "api_key in config file secret/config.json must be a string");
}

TEST_F(ClientConfigLoaderTests, ArrayFieldIsWrongType)
{
	const auto config = parse(R"({"base_url": ["https://a/api", "https://b/api"], "api_key": "chk_test"})");

	ASSERT_FALSE(config.has_value());
	EXPECT_EQ(config.error(), "base_url in config file secret/config.json must be a string");
}

TEST_F(ClientConfigLoaderTests, NullFieldIsWrongType)
{
	const auto config = parse(R"({"base_url": null, "api_key": "chk_test"})");

	ASSERT_FALSE(config.has_value());
	EXPECT_EQ(config.error(), "base_url in config file secret/config.json must be a string");
}

TEST_F(ClientConfigLoaderTests, FieldsInsideOtherArraysAreIgnored)
{
	const auto config = parse(R"({"mirrors": ["https://mirror/api", {"base_url": "https://proxy/api"}],)"
	                          R"("base_url": "https://localhost/api", "api_key": "chk_test"})");

	ASSERT_TRUE(config.has_value()) << config.error();
	EXPECT_EQ(config->host, "localhost");
}

TEST_F(ClientConfigLoaderTests, KeyWithLineBreakIsError)
{
	const auto config = parse(R"({"base_url": "https://localhost/api", "api_key": "chk_test\r\nX-Injected: 1"})");

	ASSERT_FALSE(config.has_value());
	EXPECT_EQ(config.error(), "api_key must contain only printable ASCII characters without spaces");
}

TEST_F(ClientConfigLoaderTests, KeyWithSpaceOrNonAsciiIsError)
{
	EXPECT_FALSE(parse(VALID_CONFIG, {.key = "chk test"}).has_value());
	EXPECT_FALSE(parse(VALID_CONFIG, {.key = "chk_test "}).has_value());
	EXPECT_FALSE(parse(VALID_CONFIG, {.key = "\xd0\xba\xd0\xbb\xd1\x8e\xd1\x87"}).has_value());
	EXPECT_FALSE(parse(VALID_CONFIG, {.key = std::string("chk\0test", 8)}).has_value());
}

TEST_F(ClientConfigLoaderTests, KeyWithEveryPrintableAsciiCharacterIsAccepted)
{
	EXPECT_TRUE(parse(VALID_CONFIG, {.key = R"(!"#$%&'()*+,-./0-9:;<=>?@A-Z[\]^_`a-z{|}~)"}).has_value());
}
