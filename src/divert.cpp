// Solar PV power diversion
// Modulate charge rate based on solar PV output
// Glyn Hudson | OpenEnergyMonitor.org

#include <Arduino.h>
#include "emonesp.h"
#include "input.h"
#include "config.h"
#include "RapiSender.h"
#include "mqtt.h"
#include "event.h"
#include "openevse.h"

// 1: Normal / Fast Charge (default):
// Charging at maximum rate irrespective of solar PV / grid_ie output

// 2: Eco :
// Either modulate charge rate based solar PV generation (if only solar PV feed is available)

// Or modulate charge rate based on on excess power (if grid feed (positive import / negative export) is available) i.e. power that would otherwise be exported to the grid is diverted to EVSE.
// Note: it's Assumed EVSE power is included in grid feed e.g. (charge rate = gen - use - EVSE).

// If EVSE is sleeping charging will not start until solar PV / excess power > min chanrge rate
// Once charging begins it will not pause even if solaer PV / excess power drops less then minimm charge rate. This avoids wear on the relay and the car

#define SERVICE_LEVEL1_VOLTAGE  110
#define SERVICE_LEVEL2_VOLTAGE  240

#define DIVERT_MODE_NORMAL      1
#define DIVERT_MODE_ECO         2

#define GRID_IE_RESERVE_POWER   100.0

// Default to normal charging unless set. Divert mode always defaults back to 1 if unit is reset (divertmode not saved in EEPROM)
byte divertmode = DIVERT_MODE_NORMAL;     // default normal mode
int solar = 0;
int grid_ie = 0;
byte min_charge_current = 6;      // TO DO: set to be min charge current as set on the OpenEVSE e.g. "$GC min-current max-current"
byte max_charge_current = 32;     // TO DO: to be set to be max charge current as set on the OpenEVSE e.g. "$GC min-current max-current"
int charge_rate = 0;
int last_state = OPENEVSE_STATE_INVALID;

extern RapiSender rapiSender;

// Update divert mode e.g. Normal / Eco
// function called when divert mode is changed
void divertmode_update(byte newmode)
{
  DBUGF("Set divertmode: %d", newmode);
  if(divertmode != newmode)
  {
    divertmode = newmode;

    // restore max charge current if normal mode or zero if eco mode
    switch(divertmode)
    {
      case DIVERT_MODE_NORMAL:
        // Restore the max charge current
        rapiSender.sendCmd(String(F("$SC ")) + String(max_charge_current));
        break;

      case DIVERT_MODE_ECO:
        charge_rate = 0;
        // Read the current charge current, assume this is the max set by the user
        if(0 == rapiSender.sendCmd(F("$GE"))) {
          max_charge_current = String(rapiSender.getToken(1)).toInt();
        }
        break;

      default:
        return;
    }

    String event = F("{\"divertmode\":");
    event += String(divertmode);
    event += F("}");
    event_send(event);
  }
}

void divert_current_loop()
{
  Profile_Start(divert_current_loop);

  if(last_state != state)
  {
    DBUGVAR(last_state);
    DBUGVAR(state);
    DBUGVAR(divertmode);

    // Revert to normal mode on disconnecting the car
    if(OPENEVSE_STATE_NOT_CONNECTED == state && DIVERT_MODE_ECO == divertmode) {
      divertmode_update(DIVERT_MODE_NORMAL);
    }
    last_state = state;
  }

  Profile_End(divert_current_loop, 5);
} //end divert_current_loop

// Set charge rate depending on divert mode and solar / grid_ie
void divert_update_state()
{
  Profile_Start(divert_update_state);

  // If divert mode = Eco (2)
  if (divertmode == DIVERT_MODE_ECO)
  {
    int current_charge_rate;

    // Read the current charge rate
    if(0 == rapiSender.sendCmd(F("$GE"))) {
      current_charge_rate = String(rapiSender.getToken(1)).toInt();
      DBUGVAR(current_charge_rate);
    }

    // IMPROVE: Read from OpenEVSE or emonTX (MQTT)
    int voltage = 1 == service ? SERVICE_LEVEL1_VOLTAGE : SERVICE_LEVEL2_VOLTAGE;

    // Calculate current
    if (mqtt_grid_ie != "")
    {
      // if grid feed is available and exporting increment the charge rate,
      // if importing drop the charge rate.
      // grid_ie is negative when exporting
      // If grid feeds is available and exporting (negative)

      double Igrid_ie = (double)grid_ie / (double)voltage;
      DBUGVAR(Igrid_ie);

      // Subtract the current charge the EV is using from the Grid IE
      if(0 == rapiSender.sendCmd(F("$GG"))) {
        int milliAmps = String(rapiSender.getToken(1)).toInt();
        double amps = (double)milliAmps / 1000.0;
        DBUGVAR(amps);
        Igrid_ie -= amps;
        DBUGVAR(Igrid_ie);
      }

      if (Igrid_ie < 0)
      {
        // If excess power
        double reserve = GRID_IE_RESERVE_POWER / (double)voltage;
        DBUGVAR(reserve);
        charge_rate = (int)floor(-Igrid_ie - reserve);
      }
      else
      {
        // no excess, so use the min charge
        charge_rate = 0;
      }
    }
    else if (mqtt_solar!="")
    {
      // if grid feed is not available: charge rate = solar generation

      double Isolar = (double)solar / (double)voltage;
      DBUGVAR(Isolar);
      charge_rate = (int)floor(Isolar);
    }

    if(OPENEVSE_STATE_SLEEPING != state) {
      // If we are not sleeping, make sure we are the minimum current
      charge_rate = max(charge_rate, min_charge_current);
    }

    DBUGVAR(charge_rate);

    if(charge_rate >= min_charge_current)
    {
      // Cap the charge rate at the configured maximum
      charge_rate = min(charge_rate, max_charge_current);

      // Change the charge rate is needed
      if(current_charge_rate != charge_rate)
      {
        // Set charge rate via RAPI
        if(0 == rapiSender.sendCmd(String(F("$SC ")) + String(charge_rate))) {
          DBUGF("Charge rate set to %d", charge_rate);
          pilot = charge_rate;
        }
      }

      // If charge rate > min current and EVSE is sleeping then start charging
      if (state == OPENEVSE_STATE_SLEEPING){
        DBUGLN(F("Wake up EVSE"));
        if(0 == rapiSender.sendCmd(F("$FE"))) {
          DBUGLN(F("Starting charge"));
        }
      }
    }
  } // end ecomode

  Profile_End(divert_update_state, 5);
} //end divert_update_state
