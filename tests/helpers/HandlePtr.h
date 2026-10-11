#ifndef WINSPIALERTCLIENT_TESTS_HANDLEPTR_H
#define WINSPIALERTCLIENT_TESTS_HANDLEPTR_H

#include <memory>

#include <windows.h>

namespace test_helpers
{

struct HandleDeleter
{
	void operator()(HANDLE handle) const noexcept
	{
		CloseHandle(handle);
	}
};

using HandlePtr = std::unique_ptr<void, HandleDeleter>;

} // namespace test_helpers

#endif // WINSPIALERTCLIENT_TESTS_HANDLEPTR_H
