#include <gtest/gtest.h>

#include <string>

#include "tests/helpers/TempFile.h"
#include "utils/FileUtils.h"

using namespace std::string_literals;
using test_helpers::TempFile;
using utils::FileUtils;

#define MAX_SIZE 16

TEST(FileUtilsTests, ReadsBinaryFile)
{
	const std::string content = "\x89PNG\r\n\x1a\n\0\xff"s;
	const TempFile file("a.png", content);

	const auto data = FileUtils::read_file(file.path(), MAX_SIZE);

	ASSERT_TRUE(data.has_value());
	EXPECT_EQ(*data, content);
}

TEST(FileUtilsTests, ReadsFileOfExactlyMaxSize)
{
	const TempFile file("a.png", std::string(MAX_SIZE, 'x'));

	EXPECT_TRUE(FileUtils::read_file(file.path(), MAX_SIZE).has_value());
}

TEST(FileUtilsTests, FileLargerThanMaxSizeIsNotRead)
{
	const TempFile file("a.png", std::string(MAX_SIZE + 1, 'x'));

	EXPECT_FALSE(FileUtils::read_file(file.path(), MAX_SIZE).has_value());
}

TEST(FileUtilsTests, ReadsEmptyFile)
{
	const TempFile file("a.png", "");

	const auto data = FileUtils::read_file(file.path(), MAX_SIZE);

	ASSERT_TRUE(data.has_value());
	EXPECT_TRUE(data->empty());
}

TEST(FileUtilsTests, MissingFileIsNotRead)
{
	EXPECT_FALSE(FileUtils::read_file("Z:\\no\\such\\file.png", MAX_SIZE).has_value());
}

TEST(FileUtilsTests, FileNameStripsDirectories)
{
	EXPECT_EQ(FileUtils::file_name("C:\\images\\capture_0001.jpg"), "capture_0001.jpg");
	EXPECT_EQ(FileUtils::file_name("images/a.png"), "a.png");
	EXPECT_EQ(FileUtils::file_name("a.png"), "a.png");
}
