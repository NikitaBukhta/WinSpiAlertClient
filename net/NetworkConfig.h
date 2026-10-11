#ifndef WINSPIALERTCLIENT_NETWORKCONFIG_H
#define WINSPIALERTCLIENT_NETWORKCONFIG_H

#include <chrono>

#include "utils/SizeUnits.h"

#define USER_AGENT "WinSpiAlertClient/1.0"

#define RESOLVE_TIMEOUT_MS 10'000
#define CONNECT_TIMEOUT_MS 10'000
#define SEND_TIMEOUT_MS 30'000
#define RECEIVE_TIMEOUT_MS 30'000
#define READ_CHUNK_SIZE (8 * KIB)

#define MAX_SEND_ATTEMPTS 3
#define NETWORK_BACKOFF_BASE std::chrono::seconds{1}
#define DEFAULT_RETRY_AFTER std::chrono::seconds{5}

#endif // WINSPIALERTCLIENT_NETWORKCONFIG_H
