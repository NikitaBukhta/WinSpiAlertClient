#ifndef WINSPIALERTCLIENT_MULTIPARTBUILDER_H
#define WINSPIALERTCLIENT_MULTIPARTBUILDER_H

#include <string>
#include <string_view>

namespace net
{

class MultipartBuilder
{
public:
	static std::string file_part_header(std::string_view boundary, std::string_view field_name,
	                                    std::string_view file_name, std::string_view content_type);
	static std::string closing_boundary(std::string_view boundary);
	static std::string content_type(std::string_view boundary);
};

} // namespace net

#endif // WINSPIALERTCLIENT_MULTIPARTBUILDER_H
