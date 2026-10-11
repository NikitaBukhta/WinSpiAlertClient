#ifndef WINSPIALERTCLIENT_TESTS_TESTCONSTANTS_H
#define WINSPIALERTCLIENT_TESTS_TESTCONSTANTS_H

#include "model/ImageConstants.h"

#define TEST_USER_AGENT "WinSpiAlertClient-tests/1.0"
#define LOCAL_HOST "127.0.0.1"
// a loopback address that is not in the mock certificate (it covers localhost and 127.0.0.1 only)
#define HOST_NOT_IN_CERTIFICATE "127.0.0.2"
#define HTTPS_DEFAULT_PORT 443
#define HTTP_DEFAULT_PORT 80

#define PNG_IMAGE PNG_SIGNATURE "image-bytes"
#define LARGE_SIDE 4000u

#endif // WINSPIALERTCLIENT_TESTS_TESTCONSTANTS_H
