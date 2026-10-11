#ifndef WINSPIALERTCLIENT_TESTS_REQUESTBODY_H
#define WINSPIALERTCLIENT_TESTS_REQUESTBODY_H

#include <array>
#include <span>
#include <string>
#include <string_view>

namespace test_helpers
{

class RequestBody
{
public:
	static std::array<std::string_view, 1> from_string(const std::string_view text)
	{
		return {text};
	}

	static std::string join(std::span<const std::string_view> parts)
	{
		std::string body;
		for (const std::string_view part : parts)
			body.append(part);
		return body;
	}
};

} // namespace test_helpers

#endif // WINSPIALERTCLIENT_TESTS_REQUESTBODY_H
