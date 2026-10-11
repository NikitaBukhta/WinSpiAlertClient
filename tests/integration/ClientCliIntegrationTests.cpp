#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <string>
#include <vector>

#include "MockServerEnvironment.h"
#include "api/SpialertApi.h"
#include "api/dto/EventsListDto.h"
#include "net/WinHttpClient.h"
#include "tests/helpers/ClientProcess.h"
#include "tests/helpers/TempFile.h"
#include "utils/FileUtils.h"
#include "utils/StringUtils.h"

#define PNG_IMAGE "\x89PNG\r\n\x1a\nimage-bytes"
#define SECRET_KEY "secret-key-1"
#define EVENTS_LIMIT 20
// a loopback address that is not in the mock certificate (it covers localhost and 127.0.0.1 only)
#define HOST_NOT_IN_CERTIFICATE "127.0.0.2"

using integration::MockServerEnvironment;
using integration::MockServerTest;
using test_helpers::ClientOutput;
using test_helpers::TempFile;
using utils::StringUtils;

namespace
{

class ClientCliIntegrationTests : public MockServerTest
{
protected:
	[[nodiscard]] ClientOutput run(const std::vector<std::wstring>& args) const
	{
		ClientOutput output = _client_process.run(args);
		EXPECT_TRUE(output.started) << output.err;
		return output;
	}

	[[nodiscard]] ClientOutput run_with_config(std::vector<std::wstring> args) const
	{
		args.insert(args.end(), {L"--config", _config.path().wstring()});
		return run(args);
	}

	static std::string mock_url()
	{
		return std::format("https://{}:{}{}", MOCK_HOST, MockServerEnvironment::port(), MOCK_API_BASE_PATH);
	}

	static std::string config_json(const std::string& base_url, const std::string& api_key)
	{
		return std::format(R"({{"base_url": "{}", "api_key": "{}"}})", base_url, api_key);
	}

	static api::dto::EventsListDto list_events()
	{
		auto http_client =
			std::make_unique<net::WinHttpClient>(MOCK_HOST, TEST_USER_AGENT, MockServerEnvironment::port());
		api::SpialertApi api(std::move(http_client), MOCK_API_BASE_PATH, MOCK_API_KEY);
		return api.get_events(EVENTS_LIMIT, 0, std::nullopt).dto;
	}

	test_helpers::ClientProcess _client_process;
	TempFile _config{"config.json", config_json(mock_url(), MOCK_API_KEY)};
};

} // namespace

TEST_F(ClientCliIntegrationTests, NoArgumentsShowsHelpAndFails)
{
	const ClientOutput output = run({});

	EXPECT_EQ(output.exit_code, 1u);
	EXPECT_TRUE(output.out.empty());
	EXPECT_TRUE(output.err.starts_with("Usage: client.exe")) << output.err;
}

TEST_F(ClientCliIntegrationTests, HelpSucceedsWithoutConfig)
{
	const ClientOutput output = run({L"help"});

	EXPECT_EQ(output.exit_code, 0u);
	EXPECT_TRUE(output.out.starts_with("Usage: client.exe")) << output.out;
	EXPECT_TRUE(output.err.empty()) << output.err;
}

TEST_F(ClientCliIntegrationTests, StatusReadsConfigFileFromArguments)
{
	const ClientOutput output = run_with_config({L"status"});

	EXPECT_EQ(output.exit_code, 0u) << output.err;
	EXPECT_EQ(output.out, "Server is online\n");
}

TEST_F(ClientCliIntegrationTests, StatusWorksWithUrlAndKeyFlags)
{
	const ClientOutput output =
		run({L"status", L"--url", StringUtils::to_wide(mock_url()), L"--key", StringUtils::to_wide(MOCK_API_KEY)});

	EXPECT_EQ(output.exit_code, 0u) << output.err;
	EXPECT_EQ(output.out, "Server is online\n");
}

TEST_F(ClientCliIntegrationTests, KeyFlagReplacesConfigFileValue)
{
	const ClientOutput output = run_with_config({L"status", L"--key", L"" SECRET_KEY});

	EXPECT_EQ(output.exit_code, 1u);
	EXPECT_TRUE(output.out.empty()) << output.out;
	EXPECT_EQ(output.err, "Server status is unknown: 401 invalid_api_key: API key is invalid\n");
}

TEST_F(ClientCliIntegrationTests, MissingDefaultConfigFails)
{
	const ClientOutput output = run({L"status"});

	EXPECT_EQ(output.exit_code, 1u);
	EXPECT_TRUE(output.err.starts_with("error: config file secret/config.json not found")) << output.err;
}

TEST_F(ClientCliIntegrationTests, MissingConfigFromArgumentsFails)
{
	const ClientOutput output = run({L"status", L"--config", L"Z:\\no\\such\\config.json"});

	EXPECT_EQ(output.exit_code, 1u);
	EXPECT_TRUE(output.err.starts_with("error: config file Z:\\no\\such\\config.json not found")) << output.err;
}

TEST_F(ClientCliIntegrationTests, HttpUrlIsRejected)
{
	const TempFile config("config.json", config_json("http://127.0.0.1:1/api/v1", SECRET_KEY));

	const ClientOutput output = run({L"status", L"--config", config.path().wstring()});

	EXPECT_EQ(output.exit_code, 1u);
	EXPECT_TRUE(output.err.starts_with("error: base_url must use https")) << output.err;
}

TEST_F(ClientCliIntegrationTests, StatusFailsWhenServerIsDown)
{
	const unsigned short free_port = test_helpers::MockServerProcess::find_free_port(MOCK_HOST);
	const std::string url = std::format("https://{}:{}{}", MOCK_HOST, free_port, MOCK_API_BASE_PATH);
	const TempFile config("config.json", config_json(url, MOCK_API_KEY));

	const ClientOutput output = run({L"status", L"--config", config.path().wstring()});

	EXPECT_EQ(output.exit_code, 1u);
	EXPECT_TRUE(output.out.empty()) << output.out;
	EXPECT_TRUE(output.err.starts_with("Network error, retrying in 1 s\nNetwork error, retrying in 2 s\n"))
	    << output.err;
	EXPECT_TRUE(output.err.contains("Server status is unknown: network error: ")) << output.err;
	EXPECT_TRUE(output.err.contains("(error 12029)")) << output.err;
}

TEST_F(ClientCliIntegrationTests, SendUploadsImage)
{
	const TempFile image("capture_0001.png", PNG_IMAGE);

	const ClientOutput output = run_with_config({L"send", image.path().wstring()});

	EXPECT_EQ(output.exit_code, 0u) << output.err;
	EXPECT_TRUE(output.out.starts_with("Sent ")) << output.out;
	const auto events = list_events();
	ASSERT_EQ(events.items.size(), 1u);
	EXPECT_EQ(events.items[0].file_name, "capture_0001.png");
	EXPECT_TRUE(events.items[0].image_uploaded);
}

TEST_F(ClientCliIntegrationTests, SendTellsTheUserItWaitsForRateLimit)
{
	add_fault(R"({"fault":"status","status":429,"retry_after":1,"method":"POST","path":"/api/v1/events"})");
	const TempFile image("capture_0001.png", PNG_IMAGE);

	const ClientOutput output = run_with_config({L"send", image.path().wstring()});

	EXPECT_EQ(output.exit_code, 0u) << output.err;
	EXPECT_EQ(output.err, "Rate limited, retrying in 1 s\n");
	EXPECT_TRUE(output.out.starts_with("Sent ")) << output.out;
	EXPECT_EQ(list_events().total, 1);
}

TEST_F(ClientCliIntegrationTests, SendDoesNotWaitRetryAfterOverTheLimit)
{
	add_fault(R"({"fault":"status","status":429,"retry_after":3600,"method":"POST","path":"/api/v1/events"})");
	const TempFile image("capture_0001.png", PNG_IMAGE);

	const ClientOutput output = run_with_config({L"send", image.path().wstring()});

	EXPECT_EQ(output.exit_code, 1u);
	EXPECT_FALSE(output.err.contains("retrying")) << output.err;
	EXPECT_TRUE(output.err.contains("(server asks to wait 3600 s, more than 60 s)")) << output.err;
	EXPECT_TRUE(list_events().items.empty());
}

TEST_F(ClientCliIntegrationTests, RejectedCertificateIsNotRetried)
{
	const unsigned short port = test_helpers::MockServerProcess::find_free_port(HOST_NOT_IN_CERTIFICATE);
	const test_helpers::MockServerProcess mock(HOST_NOT_IN_CERTIFICATE, port);
	ASSERT_TRUE(mock.is_listening());
	const std::string url = std::format("https://{}:{}{}", HOST_NOT_IN_CERTIFICATE, port, MOCK_API_BASE_PATH);
	const TempFile config("config.json", config_json(url, MOCK_API_KEY));

	const ClientOutput output = run({L"status", L"--config", config.path().wstring()});

	EXPECT_EQ(output.exit_code, 1u);
	EXPECT_TRUE(output.err.starts_with("Server status is unknown: network error: ")) << output.err;
	EXPECT_TRUE(output.err.contains("(error 12175)")) << output.err;
}

TEST_F(ClientCliIntegrationTests, SendMissingImageFails)
{
	const ClientOutput output = run_with_config({L"send", L"Z:\\no\\such\\capture.png"});

	EXPECT_EQ(output.exit_code, 1u);
	EXPECT_TRUE(output.err.contains("Failed to send Z:\\no\\such\\capture.png")) << output.err;
	EXPECT_TRUE(list_events().items.empty());
}

TEST_F(ClientCliIntegrationTests, SendShowsServerErrorCodeAndMessage)
{
	add_fault(R"({"fault":"status","status":422,"code":"validation_error","message":"event_code is out of range",)"
	          R"("method":"POST","path":"/api/v1/events"})");
	const TempFile image("capture_0001.png", PNG_IMAGE);

	const ClientOutput output = run_with_config({L"send", image.path().wstring()});

	EXPECT_EQ(output.exit_code, 1u);
	const std::string expected = std::format("Failed to send {}: Event was not created: 422 validation_error: "
	                                         "event_code is out of range\n",
	                                         utils::FileUtils::to_utf8(image.path()));
	EXPECT_EQ(output.err, expected);
	EXPECT_TRUE(list_events().items.empty());
}

TEST_F(ClientCliIntegrationTests, StatusShowsHtmlErrorPageAsStatusCode)
{
	add_fault(R"({"fault":"html","method":"GET","path":"/api/v1/status"})");

	const ClientOutput output = run_with_config({L"status"});

	EXPECT_EQ(output.exit_code, 1u);
	EXPECT_EQ(output.err, "Server status is unknown: 502 Bad Gateway (response is not a JSON error)\n");
}

TEST_F(ClientCliIntegrationTests, UnknownCommandFailsWithHint)
{
	const ClientOutput output = run({L"upload"});

	EXPECT_EQ(output.exit_code, 1u);
	EXPECT_EQ(output.err, "error: unknown command 'upload'\nSee 'client.exe help'.\n");
}

TEST_F(ClientCliIntegrationTests, KeyNeverAppearsInOutput)
{
	const TempFile http_config("config.json", config_json("http://127.0.0.1:1/api/v1", SECRET_KEY));
	const std::vector<ClientOutput> outputs = {
		run_with_config({L"status"}),
		run_with_config({L"send", L"Z:\\no\\such\\capture.png"}),
		run_with_config({L"status", L"--key", L"" SECRET_KEY}),
		run({L"status", L"--config", http_config.path().wstring()}),
	};

	for (const ClientOutput& output : outputs)
	{
		EXPECT_FALSE(output.out.contains(MOCK_API_KEY)) << output.out;
		EXPECT_FALSE(output.err.contains(MOCK_API_KEY)) << output.err;
		EXPECT_FALSE(output.out.contains(SECRET_KEY)) << output.out;
		EXPECT_FALSE(output.err.contains(SECRET_KEY)) << output.err;
	}
}
