#ifndef WINSPIALERTCLIENT_IMAGE_H
#define WINSPIALERTCLIENT_IMAGE_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace model
{

class Image
{
public:
	Image(std::string file_name, std::string data);

	static std::optional<Image> load(const std::filesystem::path& path, std::uintmax_t max_size);

	[[nodiscard]] const std::string& file_name() const noexcept;
	[[nodiscard]] const std::string& data() const noexcept;
	[[nodiscard]] size_t size() const noexcept;
	[[nodiscard]] std::optional<std::string_view> content_type() const;
	[[nodiscard]] std::optional<std::string> validation_error() const;

private:
	struct Format
	{
		std::string_view extension;
		std::string_view content_type;
		std::string_view signature; // NOTE: https://en.wikipedia.org/wiki/List_of_file_signatures
	};

	[[nodiscard]] const Format* find_format() const;
	[[nodiscard]] bool is_valid() const;
	[[nodiscard]] bool has_valid_name() const;
	static bool is_name_char(char c);
	static std::string supported_extensions();

private:
	std::string _file_name;
	std::string _data;

	static constexpr auto SUPPORTED_FORMATS = std::to_array<Format>({
		{".png", "image/png", "\x89PNG\r\n\x1a\n"},
		{".jpg", "image/jpeg", "\xff\xd8\xff"},
		{".jpeg", "image/jpeg", "\xff\xd8\xff"},
	});
};
} // namespace model

#endif // WINSPIALERTCLIENT_IMAGE_H
