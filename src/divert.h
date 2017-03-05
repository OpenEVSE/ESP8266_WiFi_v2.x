// Solar PV power diversion
// Modulate charge rate based on solar PV output
// Glyn Hudson | OpenEnergyMonitor.org

#ifndef _EMONESP_DIVERT_H
#define _EMONESP_DIVERT_H

#include <Arduino.h>

// global variable
extern byte mode;

// Change mode
void change_mode(byte mode);

#endif // _EMONESP_DIVERT_H