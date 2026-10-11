#include "MultipartBuilder.h"

#include <format>

#define CRLF "\r\n"

namespace net
{

std::string MultipartBuilder::file_part_header(std::string_view boundary, std::string_view field_name,
                                               std::string_view file_name, std::string_view content_type)
{
	return std::format("--{}" CRLF "Content-Disposition: form-data; name=\"{}\"; filename=\"{}\"" CRLF
	                   "Content-Type: {}" CRLF CRLF,
	                   boundary, field_name, file_name, content_type);
}

std::string MultipartBuilder::closing_boundary(std::string_view boundary)
{
	return std::format(CRLF "--{}--" CRLF, boundary);
}

std::string MultipartBuilder::content_type(std::string_view boundary)
{
	return std::format("multipart/form-data; boundary={}", boundary);
}

} // namespace net
