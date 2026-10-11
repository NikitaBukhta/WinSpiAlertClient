#include "UuidGenerator.h"

#include <windows.h>
#include <rpc.h>

namespace utils
{

std::string UuidGenerator::generate()
{
	UUID uuid{};
	const RPC_STATUS status = UuidCreate(&uuid);
	const bool created = status == RPC_S_OK || status == RPC_S_UUID_LOCAL_ONLY;
	if (!created)
		return {};

	RPC_CSTR text = nullptr;
	if (UuidToStringA(&uuid, &text) != RPC_S_OK)
		return {};

	std::string result(reinterpret_cast<const char*>(text));
	RpcStringFreeA(&text);
	return result;
}

} // namespace utils
