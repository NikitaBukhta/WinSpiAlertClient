#ifndef WINSPIALERTCLIENT_EVENTSLISTDTO_H
#define WINSPIALERTCLIENT_EVENTSLISTDTO_H

#include <optional>
#include <vector>

#include "EventObjectDto.h"

namespace api::dto
{

struct EventsListDto
{
	int total{};
	int limit{};
	int offset{};
	std::vector<EventObjectDto> items;
};

// NOTE: top-level fields are handled here, every object inside "items" is delegated to EventObjectHandler
struct EventsListHandler : utils::BaseSaxHandler<EventsListHandler>
{
	EventsListDto& dto;

	int depth{}; // object nesting level: 1 = root, 2 = item of "items"
	bool in_items{};
	std::optional<EventObjectHandler> item;

	explicit EventsListHandler(EventsListDto& target_dto) : dto(target_dto)
	{
	}

	bool Key(const char* str, rapidjson::SizeType length, bool copy)
	{
		if (item)
			return item->Key(str, length, copy);
		return BaseSaxHandler::Key(str, length, copy);
	}

	bool StartObject()
	{
		++depth;
		if (in_items && depth == 2)
		{
			dto.items.emplace_back();
			item.emplace(dto.items.back());
		}
		return true;
	}

	bool EndObject(rapidjson::SizeType member_count)
	{
		if (depth == 2)
			item.reset();
		--depth;
		return true;
	}

	bool StartArray()
	{
		if (depth == 1 && current_key == "items")
			in_items = true;
		return true;
	}

	bool EndArray(rapidjson::SizeType element_count)
	{
		if (!item)
			in_items = false;
		return true;
	}

	bool Bool(bool val)
	{
		if (item)
			return item->Bool(val);
		return true;
	}

	bool Int(int val)
	{
		if (item)
			return item->Int(val);

		if (current_key == "total")
			dto.total = val;
		else if (current_key == "limit")
			dto.limit = val;
		else if (current_key == "offset")
			dto.offset = val;
		return true;
	}

	bool Uint(unsigned val)
	{
		if (item)
			return item->Uint(val);
		return Int(static_cast<int>(val));
	}

	bool Uint64(uint64_t val)
	{
		if (item)
			return item->Uint64(val);
		return true;
	}

	bool String(const char* str, rapidjson::SizeType length, bool copy)
	{
		if (item)
			return item->String(str, length, copy);
		return true;
	}
};

class EventsListMapper
{
public:
	// nullopt if the body is not valid JSON
	static std::optional<EventsListDto> fromJson(char* json_buffer)
	{
		return utils::SaxParser::parse_in_situ<EventsListHandler, EventsListDto>(json_buffer);
	}
};

} // namespace api::dto

#endif // WINSPIALERTCLIENT_EVENTSLISTDTO_H
