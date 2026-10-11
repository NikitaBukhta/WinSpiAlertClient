#ifndef WINSPIALERTCLIENT_SAXHANDLER_H
#define WINSPIALERTCLIENT_SAXHANDLER_H

#include <string_view>

#include "rapidjson/reader.h"

namespace utils
{

// NOTE: SAX parser https://rapidjson.org/md_doc_sax.html
template <typename Derived>
struct BaseSaxHandler
{
	std::string_view current_key;

	using Ch = char;

	bool Key(const char* str, rapidjson::SizeType length, bool copy)
	{
		current_key = std::string_view(str, length);
		return true;
	}

	bool Null()
	{
		return true;
	}

	bool Bool(bool b)
	{
		return true;
	}

	bool Int(int i)
	{
		return true;
	}

	bool Uint(unsigned u)
	{
		return true;
	}

	bool Int64(int64_t i)
	{
		return true;
	}

	bool Uint64(uint64_t u)
	{
		return true;
	}

	bool Double(double d)
	{
		return true;
	}

	bool RawNumber(const char* str, rapidjson::SizeType length, bool copy)
	{
		return true;
	}

	bool String(const char* str, rapidjson::SizeType length, bool copy)
	{
		return true;
	}

	bool StartObject()
	{
		return true;
	}

	bool EndObject(rapidjson::SizeType memberCount)
	{
		return true;
	}

	bool StartArray()
	{
		return true;
	}

	bool EndArray(rapidjson::SizeType elementCount)
	{
		return true;
	}
};

} // namespace utils

#endif // WINSPIALERTCLIENT_SAXHANDLER_H
