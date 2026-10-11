#include "ComScope.h"

#include <windows.h>
#include <objbase.h>

namespace utils
{

ComScope::ComScope()
{
	const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	// NOTE: RPC_E_CHANGED_MODE means COM already runs in another mode. It is usable, but not ours to uninitialize.
	_must_uninitialize = SUCCEEDED(result);
	_initialized = _must_uninitialize || result == RPC_E_CHANGED_MODE;
}

ComScope::~ComScope()
{
	if (_must_uninitialize)
		CoUninitialize();
}

bool ComScope::is_initialized() const noexcept
{
	return _initialized;
}

} // namespace utils
