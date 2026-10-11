#include <gtest/gtest.h>

#include <expected>
#include <string>
#include <vector>

#include "cli/CommandLineParser.h"

using cli::CommandLine;
using cli::CommandLineParser;
using CommandEnum = cli::CommandLine::CommandEnum;

namespace
{

class CommandLineParserTests : public ::testing::Test
{
protected:
	static std::expected<CommandLine, std::string> parse(const std::vector<const wchar_t*>& args)
	{
		return CommandLineParser(args).parse();
	}
};

} // namespace

TEST_F(CommandLineParserTests, ParsesStatus)
{
	const auto command_line = parse({L"status"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->command, CommandEnum::STATUS_COMMAND);
	EXPECT_FALSE(command_line->source.config_path.has_value());
	EXPECT_FALSE(command_line->source.url.has_value());
	EXPECT_FALSE(command_line->source.key.has_value());
}

TEST_F(CommandLineParserTests, ParsesSendWithImagePath)
{
	const auto command_line = parse({L"send", L"C:\\images\\фото 1.png"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->command, CommandEnum::SEND_COMMAND);
	EXPECT_EQ(command_line->image_path, std::filesystem::path(L"C:\\images\\фото 1.png"));
}

TEST_F(CommandLineParserTests, ParsesList)
{
	const auto command_line = parse({L"list"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->command, CommandEnum::LIST_COMMAND);
	EXPECT_FALSE(command_line->pending);
}

TEST_F(CommandLineParserTests, ParsesListPending)
{
	const auto command_line = parse({L"list", L"--pending"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->command, CommandEnum::LIST_COMMAND);
	EXPECT_TRUE(command_line->pending);
}

TEST_F(CommandLineParserTests, ParsesHelpInEveryForm)
{
	for (const wchar_t* help : {L"help", L"--help", L"-h"})
	{
		const auto command_line = parse({help});

		ASSERT_TRUE(command_line.has_value()) << command_line.error();
		EXPECT_EQ(command_line->command, CommandEnum::HELP_COMMAND);
	}
}

TEST_F(CommandLineParserTests, HelpFlagWinsOverOtherArguments)
{
	const auto command_line = parse({L"send", L"--help"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->command, CommandEnum::HELP_COMMAND);
}

TEST_F(CommandLineParserTests, ParsesSource)
{
	const auto command_line = parse({L"--config", L"C:\\cfg\\mock.json", L"--url", L"https://localhost:8443/api/v1",
	                                 L"--key", L"test-key", L"status"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->source.config_path, std::filesystem::path(L"C:\\cfg\\mock.json"));
	EXPECT_EQ(command_line->source.url, "https://localhost:8443/api/v1");
	EXPECT_EQ(command_line->source.key, "test-key");
}

TEST_F(CommandLineParserTests, OptionsMayStandAfterCommand)
{
	const auto command_line = parse({L"send", L"a.png", L"--key", L"test-key", L"--url", L"https://localhost/api"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->command, CommandEnum::SEND_COMMAND);
	EXPECT_EQ(command_line->image_path, std::filesystem::path(L"a.png"));
	EXPECT_EQ(command_line->source.key, "test-key");
	EXPECT_EQ(command_line->source.url, "https://localhost/api");
}

TEST_F(CommandLineParserTests, ConvertsSourceToUtf8)
{
	const auto command_line = parse({L"--key", L"ключ", L"status"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->source.key, "\xd0\xba\xd0\xbb\xd1\x8e\xd1\x87");
}

TEST_F(CommandLineParserTests, EmptyArgumentsAreError)
{
	const auto command_line = parse({});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "no command given");
}

TEST_F(CommandLineParserTests, OnlyOptionsAreError)
{
	const auto command_line = parse({L"--url", L"https://localhost/api"});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "no command given");
}

TEST_F(CommandLineParserTests, UnknownCommandIsError)
{
	const auto command_line = parse({L"upload"});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "unknown command 'upload'");
}

TEST_F(CommandLineParserTests, CommandsAreCaseSensitive)
{
	EXPECT_FALSE(parse({L"Status"}).has_value());
}

TEST_F(CommandLineParserTests, SendWithoutImageIsError)
{
	const auto command_line = parse({L"send"});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "send requires <image>");
}

TEST_F(CommandLineParserTests, ExtraArgumentIsError)
{
	const auto send = parse({L"send", L"a.png", L"b.png"});
	const auto status = parse({L"status", L"now"});

	ASSERT_FALSE(send.has_value());
	ASSERT_FALSE(status.has_value());
	EXPECT_EQ(send.error(), "unexpected argument 'b.png'");
	EXPECT_EQ(status.error(), "unexpected argument 'now'");
}

TEST_F(CommandLineParserTests, UnknownOptionIsError)
{
	const auto command_line = parse({L"status", L"--verbose"});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "unknown option --verbose");
}

TEST_F(CommandLineParserTests, EqualsSyntaxIsNotSupported)
{
	const auto command_line = parse({L"--url=https://localhost/api", L"status"});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "unknown option --url=https://localhost/api");
}

TEST_F(CommandLineParserTests, OptionWithoutValueIsError)
{
	const auto command_line = parse({L"status", L"--url"});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "--url requires a value");
}

TEST_F(CommandLineParserTests, OptionWithEmptyValueIsError)
{
	const auto command_line = parse({L"--key", L"", L"status"});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "--key requires a value");
}

TEST_F(CommandLineParserTests, RepeatedOptionIsError)
{
	const auto command_line = parse({L"--key", L"a", L"--key", L"b", L"status"});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "--key is given more than once");
}

TEST_F(CommandLineParserTests, ErrorsNeverContainKey)
{
	const auto repeated = parse({L"--key", L"secret-key-1", L"--key", L"secret-key-2", L"status"});
	const auto unknown = parse({L"--key", L"secret-key-1", L"upload"});

	ASSERT_FALSE(repeated.has_value());
	ASSERT_FALSE(unknown.has_value());
	EXPECT_EQ(repeated.error().find("secret-key"), std::string::npos);
	EXPECT_EQ(unknown.error().find("secret-key"), std::string::npos);
}

TEST_F(CommandLineParserTests, PendingWithoutListIsError)
{
	const auto command_line = parse({L"status", L"--pending"});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "--pending is valid only for the list command");
}

TEST_F(CommandLineParserTests, HelpWinsOverWrongOptionBeforeIt)
{
	const auto command_line = parse({L"--bogus", L"--help"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->command, CommandEnum::HELP_COMMAND);
}

TEST_F(CommandLineParserTests, HelpAsOptionValueShowsHelp)
{
	const auto command_line = parse({L"--config", L"--help"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->command, CommandEnum::HELP_COMMAND);
}

TEST_F(CommandLineParserTests, OptionIsNotTakenAsValueOfAnotherOption)
{
	const auto command_line = parse({L"--config", L"--url", L"https://localhost/api", L"status"});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "--config requires a value");
}

TEST_F(CommandLineParserTests, ImageStartingWithDashNeedsEndOfOptions)
{
	const auto command_line = parse({L"send", L"-capture.png"});

	ASSERT_FALSE(command_line.has_value());
	EXPECT_EQ(command_line.error(), "unknown option -capture.png");
}

TEST_F(CommandLineParserTests, EndOfOptionsMakesRestPositional)
{
	const auto command_line = parse({L"--key", L"test-key", L"send", L"--", L"-capture.png"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->command, CommandEnum::SEND_COMMAND);
	EXPECT_EQ(command_line->image_path, std::filesystem::path(L"-capture.png"));
	EXPECT_EQ(command_line->source.key, "test-key");
}

TEST_F(CommandLineParserTests, HelpAfterEndOfOptionsIsFileName)
{
	const auto command_line = parse({L"send", L"--", L"--help"});

	ASSERT_TRUE(command_line.has_value()) << command_line.error();
	EXPECT_EQ(command_line->command, CommandEnum::SEND_COMMAND);
	EXPECT_EQ(command_line->image_path, std::filesystem::path(L"--help"));
}
