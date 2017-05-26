#ifndef _EMONESP_H
#define _EMONESP_H

// -------------------------------------------------------------------
// General support code used by all modules
// -------------------------------------------------------------------

// Uncomment to use hardware UART 1 for debug else use UART 0
// #define DEBUG_SERIAL1

#ifdef DEBUG_SERIAL1
#define DEBUG Serial1
#else
#define DEBUG Serial
#endif

#include "debug.h"

#endif // _EMONESP_H
