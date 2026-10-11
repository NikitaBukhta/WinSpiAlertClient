#ifndef WINSPIALERTCLIENT_SERVERERRORDTO_H
#define WINSPIALERTCLIENT_SERVERERRORDTO_H

#include <string>
#include <string_view>
#include <vector>

#include "utils/SaxHandler.h"

namespace api::dto
{

struct ServerErrorDetailDto
{
	std::string field;
	std::string message;
};

struct ServerErrorDto
{
	std::string code;
	std::string message;
	std::vector<ServerErrorDetailDto> details;
};

struct ServerErrorHandler : utils::BaseSaxHandler<ServerErrorHandler>
{
	ServerErrorDto& dto;

	int depth{}; // object nesting level: 1 = root, 2 = "error", 3 = item of "details"
	bool in_error{};
	bool in_details{};

	explicit ServerErrorHandler(ServerErrorDto& target_dto) : dto(target_dto)
	{
	}

	bool StartObject()
	{
		++depth;
		if (depth == 2 && current_key == "error")
			in_error = true;
		else if (depth == 3 && in_details)
			dto.details.emplace_back();
		return true;
	}

	bool EndObject(rapidjson::SizeType member_count)
	{
		if (depth == 2)
			in_error = false;
		--depth;
		return true;
	}

	bool StartArray()
	{
		if (in_error && depth == 2 && current_key == "details")
			in_details = true;
		return true;
	}

	bool EndArray(rapidjson::SizeType element_count)
	{
		if (depth == 2)
			in_details = false;
		return true;
	}

	bool String(const char* str, const rapidjson::SizeType length, bool copy)
	{
		const std::string_view val(str, length);
		const bool in_detail_item = in_details && depth == 3 && !dto.details.empty();

		if (in_error && depth == 2)
		{
			if (current_key == "code")
				dto.code.assign(val);
			else if (current_key == "message")
				dto.message.assign(val);
		}
		else if (in_detail_item)
		{
			if (current_key == "field")
				dto.details.back().field.assign(val);
			else if (current_key == "message")
				dto.details.back().message.assign(val);
		}
		return true;
	}
};

class ServerErrorMapper
{
public:
	static ServerErrorDto fromJson(char* json_buffer)
	{
		ServerErrorDto out;
		rapidjson::Reader reader;
		rapidjson::InsituStringStream ss(json_buffer);
		ServerErrorHandler handler{out};

		reader.Parse<rapidjson::kParseInsituFlag>(ss, handler);

		return out;
	}
};

} // namespace api::dto

#endif // WINSPIALERTCLIENT_SERVERERRORDTO_H
