#ifndef WINSPIALERTCLIENT_EVENTREQUESTDTO_H
#define WINSPIALERTCLIENT_EVENTREQUESTDTO_H

#include <string>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "utils/TimeParser.h"

namespace api::dto
{

struct EventRequestDto
{
	std::string event_uid;
	int event_code{};
	std::string file_name;
	utils::time_point timestamp;
};

class EventRequestMapper
{
public:
	static std::string toJson(const EventRequestDto& dto)
	{
		rapidjson::StringBuffer buffer;
		rapidjson::Writer writer(buffer);
		const std::string timestamp = utils::TimeParser::format_iso8601(dto.timestamp);

		writer.StartObject();
		writer.Key("event_uid");
		writer.String(dto.event_uid.data(), static_cast<rapidjson::SizeType>(dto.event_uid.size()));
		writer.Key("event_code");
		writer.Int(dto.event_code);
		writer.Key("file_name");
		writer.String(dto.file_name.data(), static_cast<rapidjson::SizeType>(dto.file_name.size()));
		writer.Key("timestamp");
		writer.String(timestamp.data(), static_cast<rapidjson::SizeType>(timestamp.size()));
		writer.EndObject();

		return {buffer.GetString(), buffer.GetSize()};
	}
};

} // namespace api::dto

#endif // WINSPIALERTCLIENT_EVENTREQUESTDTO_H
