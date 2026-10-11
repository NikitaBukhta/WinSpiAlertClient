#ifndef WINSPIALERTCLIENT_FILEUTILS_H
#define WINSPIALERTCLIENT_FILEUTILS_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace utils
{

class FileUtils
{
public:
	static std::optional<std::string> read_file(const std::filesystem::path& path, std::uintmax_t max_size);
	static std::string file_name(const std::filesystem::path& path);
	static std::string to_utf8(const std::filesystem::path& path);
};
} // namespace utils

#endif // WINSPIALERTCLIENT_FILEUTILS_H
