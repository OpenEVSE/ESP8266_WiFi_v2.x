#ifndef _EMONESP_H
#define _EMONESP_H

// -------------------------------------------------------------------
// General support code used by all modules
// -------------------------------------------------------------------

#include "debug.h"
#include "profile.h"

#ifndef RAPI_PORT
#ifdef ESP32
#define RAPI_PORT Serial1
#elif defined(ESP8266)
#define RAPI_PORT Serial
#else
#error Platform not supported
#endif
#endif

#ifndef DEFAULT_VOLTAGE
#define DEFAULT_VOLTAGE  240
#endif

#ifdef NO_SENSOR_SCALING

#ifndef VOLTS_SCALE_FACTOR
#define VOLTS_SCALE_FACTOR  1.0
#endif

#ifndef AMPS_SCALE_FACTOR
#define AMPS_SCALE_FACTOR   1.0
#endif

#ifndef TEMP_SCALE_FACTOR
#define TEMP_SCALE_FACTOR   1.0
#endif

#else

#ifndef VOLTS_SCALE_FACTOR
#define VOLTS_SCALE_FACTOR  1.0
#endif

#ifndef AMPS_SCALE_FACTOR
#define AMPS_SCALE_FACTOR   1000.0
#endif

#ifndef TEMP_SCALE_FACTOR
#define TEMP_SCALE_FACTOR   10.0
#endif

#endif

#endif // _EMONESP_H
