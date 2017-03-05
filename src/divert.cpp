// Solar PV power diversion
// Modulate charge rate based on solar PV output
// Glyn Hudson | OpenEnergyMonitor.org

#include <Arduino.h>

// Charge Modes: 1:Eco 2:Eco+, 3:Normal (default)

// Default to normal charging unless set
byte mode = 3;

void change_mode(byte newmode){
  mode = newmode;
}