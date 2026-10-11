#ifndef WINSPIALERTCLIENT_EVENTOBJECTDTO_H
#define WINSPIALERTCLIENT_EVENTOBJECTDTO_H

#include <string>
#include <string_view>

#include "utils/SaxHandler.h"
#include "utils/TimeParser.h"

namespace api::dto
{

struct EventObjectDto
{
	int event_id{};
	int event_code{};
	std::string event_uid;
	utils::time_point timestamp;

	std::string file_name;
	std::string image_sha256;
	size_t image_size{};
	bool image_uploaded{};
	utils::time_point received_at;
	utils::time_point image_uploaded_at;
};

struct EventObjectHandler : utils::BaseSaxHandler<EventObjectHandler>
{
	EventObjectDto& dto;

	explicit EventObjectHandler(EventObjectDto& target_dto) : dto(target_dto)
	{
	}

	[[nodiscard]] bool Bool(const bool val) const
	{
		if (current_key == "image_uploaded")
			dto.image_uploaded = val;
		return true;
	}

	// NOTE: rapidjson reports non-negative integers via Uint/Uint64, negative ones via Int/Int64
	[[nodiscard]] bool Int(const int val) const
	{
		if (current_key == "event_id")
			dto.event_id = val;
		else if (current_key == "event_code")
			dto.event_code = val;
		return true;
	}

	[[nodiscard]] bool Uint(const unsigned val) const
	{
		if (current_key == "image_size")
			dto.image_size = val;
		else
			return Int(static_cast<int>(val));
		return true;
	}

	[[nodiscard]] bool Uint64(const uint64_t val) const
	{
		if (current_key == "image_size")
			dto.image_size = static_cast<size_t>(val);
		return true;
	}

	bool String(const char* str, const rapidjson::SizeType length, bool copy) const
	{
		const std::string_view val(str, length);

		if (current_key == "event_uid")
			dto.event_uid.assign(val);
		else if (current_key == "file_name")
			dto.file_name.assign(val);
		else if (current_key == "timestamp")
			dto.timestamp = utils::TimeParser::parse_iso8601(val);
		else if (current_key == "received_at")
			dto.received_at = utils::TimeParser::parse_iso8601(val);
		else if (current_key == "image_sha256")
			dto.image_sha256.assign(val);
		else if (current_key == "image_uploaded_at")
			dto.image_uploaded_at = utils::TimeParser::parse_iso8601(val);
		return true;
	}
};

class EventObjectMapper
{
public:
	static EventObjectDto fromJson(char* json_buffer)
	{
		EventObjectDto out;
		rapidjson::Reader reader;
		rapidjson::InsituStringStream ss(json_buffer);
		EventObjectHandler handler{out};

		reader.Parse<rapidjson::kParseInsituFlag>(ss, handler);

		return out;
	}
};

} // namespace api::dto

#endif // WINSPIALERTCLIENT_EVENTOBJECTDTO_H
