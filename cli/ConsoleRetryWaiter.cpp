#include "ConsoleRetryWaiter.h"

#include <print>
#include <thread>

namespace cli
{

void ConsoleRetryWaiter::wait(const std::chrono::seconds delay, const RetryReasonEnum reason)
{
	switch (reason)
	{
	case RetryReasonEnum::RATE_LIMIT_REASON:
		std::println(stderr, "Rate limited, retrying in {} s", delay.count());
		break;
	case RetryReasonEnum::NETWORK_REASON:
		std::println(stderr, "Network error, retrying in {} s", delay.count());
		break;
	}
	std::this_thread::sleep_for(delay);
}

} // namespace cli
