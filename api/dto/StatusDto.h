#ifndef WINSPIALERTCLIENT_STATUSDTO_H
#define WINSPIALERTCLIENT_STATUSDTO_H

#include <string>
#include <string_view>

#include "utils/SaxHandler.h"
#include "utils/TimeParser.h"

#define SERVER_STATUS_ONLINE "online"

namespace api::dto
{

struct StatusDto
{
	bool status{};
	std::string api_version;
	utils::time_point server_time;
	std::string candidate;
};

struct StatusDtoHandler : utils::BaseSaxHandler<StatusDtoHandler>
{
	StatusDto& dto;

	explicit StatusDtoHandler(StatusDto& target_dto) : dto(target_dto)
	{
	}

	bool String(const char* str, rapidjson::SizeType length, bool copy)
	{
		// Server sends "status" as a string, e.g. "online"
		if (current_key == "status")
		{
			dto.status = std::string_view(str, length) == SERVER_STATUS_ONLINE;
		}
		else if (current_key == "api_version")
		{
			dto.api_version.assign(str, length);
		}
		else if (current_key == "candidate")
		{
			dto.candidate.assign(str, length);
		}
		else if (current_key == "server_time")
		{
			dto.server_time = utils::TimeParser::parse_iso8601(std::string_view(str, length));
		}
		return true;
	}
};

class StatusMapper
{
public:
	static StatusDto fromJson(char* json_buffer)
	{
		StatusDto out;
		rapidjson::Reader reader;
		rapidjson::InsituStringStream ss(json_buffer);
		StatusDtoHandler handler{out};

		reader.Parse<rapidjson::kParseInsituFlag>(ss, handler);

		return out;
	}
};

} // namespace api::dto

#endif // WINSPIALERTCLIENT_STATUSDTO_H
