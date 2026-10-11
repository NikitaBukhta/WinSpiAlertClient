#ifndef WINSPIALERTCLIENT_TESTS_TEMPFILE_H
#define WINSPIALERTCLIENT_TESTS_TEMPFILE_H

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "utils/UuidGenerator.h"

namespace test_helpers
{

class TempFile
{
public:
	TempFile(const std::filesystem::path& file_name, const std::string_view content)
		: _directory(std::filesystem::temp_directory_path() / ("spialert-tests-" + utils::UuidGenerator::generate())),
		  _path(_directory / file_name)
	{
		std::filesystem::create_directories(_directory);
		std::ofstream(_path, std::ios::binary).write(content.data(), static_cast<std::streamsize>(content.size()));
	}

	~TempFile()
	{
		std::error_code ignored;
		std::filesystem::remove_all(_directory, ignored);
	}

	TempFile(const TempFile& other) = delete;
	TempFile& operator=(const TempFile& other) = delete;

	[[nodiscard]] const std::filesystem::path& path() const noexcept
	{
		return _path;
	}

private:
	std::filesystem::path _directory;
	std::filesystem::path _path;
};

} // namespace test_helpers

#endif // WINSPIALERTCLIENT_TESTS_TEMPFILE_H
