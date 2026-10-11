#include "FileUtils.h"

#include <filesystem>
#include <fstream>
#include <system_error>

#include "StringUtils.h"

namespace utils
{

std::optional<std::string> FileUtils::read_file(const std::filesystem::path& path, const std::uintmax_t max_size)
{
	std::error_code error;
	const auto size = std::filesystem::file_size(path, error);
	if (error || size > max_size)
		return std::nullopt;

	std::ifstream file(path, std::ios::binary);
	std::string data(static_cast<size_t>(size), '\0');
	const bool read = file && file.read(data.data(), static_cast<std::streamsize>(size));
	if (!read)
		return std::nullopt;

	return data;
}

std::string FileUtils::file_name(const std::filesystem::path& path)
{
	return to_utf8(path.filename());
}

std::string FileUtils::to_utf8(const std::filesystem::path& path)
{
	return StringUtils::to_utf8(path.native());
}

} // namespace utils
