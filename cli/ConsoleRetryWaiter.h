#ifndef WINSPIALERTCLIENT_CONSOLERETRYWAITER_H
#define WINSPIALERTCLIENT_CONSOLERETRYWAITER_H

#include "app/IRetryWaiter.h"

namespace cli
{

class ConsoleRetryWaiter : public app::IRetryWaiter
{
public:
	void wait(std::chrono::seconds delay, RetryReasonEnum reason) override;
};

} // namespace cli

#endif // WINSPIALERTCLIENT_CONSOLERETRYWAITER_H
