#ifndef WINSPIALERTCLIENT_TESTS_DTOPARSER_H
#define WINSPIALERTCLIENT_TESTS_DTOPARSER_H

#include <string>

namespace test_helpers
{

class DtoParser
{
public:
	// NOTE: Mappers parse in-situ and modify the buffer, so every call gets its own copy of the json
	template <class Mapper>
	static auto parse(std::string json)
	{
		return Mapper::fromJson(json.data());
	}
};

} // namespace test_helpers

#endif // WINSPIALERTCLIENT_TESTS_DTOPARSER_H
