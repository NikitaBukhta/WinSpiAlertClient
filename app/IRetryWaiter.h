#ifndef WINSPIALERTCLIENT_IRETRYWAITER_H
#define WINSPIALERTCLIENT_IRETRYWAITER_H

#include <chrono>
#include <cstdint>

namespace app
{

// NOTE: the pause before a resend; it is also where the user learns the client waits
class IRetryWaiter
{
public:
	enum class RetryReasonEnum : uint8_t
	{
		RATE_LIMIT_REASON,
		NETWORK_REASON,
		OFFLINE_REASON
	};

	virtual ~IRetryWaiter() = default;
	virtual void wait(std::chrono::seconds delay, RetryReasonEnum reason) = 0;
};

} // namespace app

#endif // WINSPIALERTCLIENT_IRETRYWAITER_H
