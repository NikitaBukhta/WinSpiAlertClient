#ifndef WINSPIALERTCLIENT_IMAGE_H
#define WINSPIALERTCLIENT_IMAGE_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "ImageConstants.h"

namespace model
{

class Image
{
public:
	enum class FormatEnum : uint8_t
	{
		PNG_FORMAT,
		JPEG_FORMAT,
	};

	Image(std::string file_name, std::string data);

	static std::optional<Image> load(const std::filesystem::path& path, std::uintmax_t max_size);

	[[nodiscard]] const std::string& file_name() const noexcept;
	[[nodiscard]] const std::string& data() const noexcept;
	[[nodiscard]] size_t size() const noexcept;
	[[nodiscard]] std::optional<FormatEnum> format() const;
	[[nodiscard]] std::optional<std::string_view> content_type() const;
	[[nodiscard]] std::optional<std::string> validation_error() const;

private:
	struct FormatInfo
	{
		FormatEnum format;
		std::string_view extension;
		std::string_view content_type;
		std::string_view signature;
	};

	[[nodiscard]] const FormatInfo* find_format() const;
	[[nodiscard]] bool is_valid() const;
	[[nodiscard]] bool has_valid_name() const;
	static bool is_name_char(char c);
	static std::string supported_extensions();

private:
	std::string _file_name;
	std::string _data;

	static constexpr auto SUPPORTED_FORMATS = std::to_array<FormatInfo>({
		{FormatEnum::PNG_FORMAT, ".png", "image/png", PNG_SIGNATURE},
		{FormatEnum::JPEG_FORMAT, ".jpg", "image/jpeg", JPEG_SIGNATURE},
		{FormatEnum::JPEG_FORMAT, ".jpeg", "image/jpeg", JPEG_SIGNATURE},
	});
};
} // namespace model

#endif // WINSPIALERTCLIENT_IMAGE_H
