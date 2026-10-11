#include "Image.h"

#include <algorithm>
#include <format>

#include "utils/FileUtils.h"

#define EXTENSION_SEPARATOR ", "
#define MAX_NAME_LENGTH 100

namespace model
{

Image::Image(std::string file_name, std::string data) : _file_name(std::move(file_name)), _data(std::move(data))
{
}

std::optional<Image> Image::load(const std::filesystem::path& path, std::uintmax_t max_size)
{
	auto data = utils::FileUtils::read_file(path, max_size);
	if (!data)
		return std::nullopt;

	return Image(utils::FileUtils::file_name(path), std::move(*data));
}

const std::string& Image::file_name() const noexcept
{
	return _file_name;
}

const std::string& Image::data() const noexcept
{
	return _data;
}

size_t Image::size() const noexcept
{
	return _data.size();
}

std::optional<Image::FormatEnum> Image::format() const
{
	if (!is_valid())
		return std::nullopt;

	return find_format()->format;
}

std::optional<std::string_view> Image::content_type() const
{
	if (!is_valid())
		return std::nullopt;

	return find_format()->content_type;
}

std::optional<std::string> Image::validation_error() const
{
	const FormatInfo* format = find_format();
	if (!format)
		return std::format("{}: unsupported extension, expected {}", _file_name, supported_extensions());
	if (!has_valid_name())
		return std::format("{}: the name before the extension must be 1-{} characters from A-Z a-z 0-9 _ -", _file_name,
		                   MAX_NAME_LENGTH);
	if (!_data.starts_with(format->signature))
		return std::format("{}: content does not match the {} extension", _file_name, format->extension);

	return std::nullopt;
}

const Image::FormatInfo* Image::find_format() const
{
	const size_t dot = _file_name.rfind('.');
	const bool has_extension = dot != std::string::npos && dot > 0;
	const std::string_view extension = has_extension ? std::string_view(_file_name).substr(dot) : std::string_view{};

	for (const FormatInfo& format : SUPPORTED_FORMATS)
	{
		if (extension == format.extension)
			return &format;
	}
	return nullptr;
}

bool Image::is_valid() const
{
	const FormatInfo* format = find_format();
	return format && has_valid_name() && _data.starts_with(format->signature);
}

bool Image::has_valid_name() const
{
	const std::string_view name = std::string_view(_file_name).substr(0, _file_name.rfind('.'));
	const bool length_valid = !name.empty() && name.size() <= MAX_NAME_LENGTH;
	return length_valid && std::ranges::all_of(name, is_name_char);
}

bool Image::is_name_char(const char c)
{
	const bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
	const bool digit = c >= '0' && c <= '9';
	return letter || digit || c == '_' || c == '-';
}

std::string Image::supported_extensions()
{
	std::string extensions;
	for (const FormatInfo& format : SUPPORTED_FORMATS)
	{
		if (!extensions.empty())
			extensions += EXTENSION_SEPARATOR;
		extensions += format.extension;
	}
	return extensions;
}

} // namespace model
