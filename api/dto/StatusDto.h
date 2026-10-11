#ifndef WINSPIALERTCLIENT_STATUSDTO_H
#define WINSPIALERTCLIENT_STATUSDTO_H

#include <optional>
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
	// nullopt if the body is not valid JSON
	static std::optional<StatusDto> fromJson(char* json_buffer)
	{
		return utils::SaxParser::parse_in_situ<StatusDtoHandler, StatusDto>(json_buffer);
	}
};

} // namespace api::dto

#endif // WINSPIALERTCLIENT_STATUSDTO_H
