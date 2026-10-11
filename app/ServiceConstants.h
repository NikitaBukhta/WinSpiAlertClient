#ifndef WINSPIALERTCLIENT_SERVICECONSTANTS_H
#define WINSPIALERTCLIENT_SERVICECONSTANTS_H

#include "utils/SizeUnits.h"

#define MIN_EVENT_CODE 1
#define MAX_EVENT_CODE 10
// NOTE: every page counts against the limit of 60 requests per minute
#define MAX_LIST_PAGES 20
// NOTE: an image over MAX_IMAGE_SIZE is still read if shrinking can bring it under the limit
#define COMPRESSION_HEADROOM (20 * MIB)

#endif // WINSPIALERTCLIENT_SERVICECONSTANTS_H
