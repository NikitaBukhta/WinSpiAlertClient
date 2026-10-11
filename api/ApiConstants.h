#ifndef WINSPIALERTCLIENT_APICONSTANTS_H
#define WINSPIALERTCLIENT_APICONSTANTS_H

#include "utils/SizeUnits.h"

#define API_KEY_HEADER "X-API-Key: "
#define STATUS_PATH "/status"
#define EVENTS_PATH "/events"

#define MAX_IMAGE_SIZE (5 * MIB)
// NOTE: GET /events accepts a limit from 1 to 100 and uses 20 when it is not given
#define MAX_EVENTS_LIMIT 100
#define DEFAULT_EVENTS_LIMIT 20

#endif // WINSPIALERTCLIENT_APICONSTANTS_H
