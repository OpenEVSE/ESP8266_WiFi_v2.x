/*
 * Copyright (c) 2015-2016 Chris Howell
 *
 * -------------------------------------------------------------------
 *
 * Additional Adaptation of OpenEVSE ESP Wifi
 * by Trystan Lea, Glyn Hudson, OpenEnergyMonitor
 * All adaptation GNU General Public License as below.
 *
 * -------------------------------------------------------------------
 *
 * This file is part of Open EVSE.
 * Open EVSE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * Open EVSE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with Open EVSE; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <Arduino.h>
#include <ArduinoOTA.h>               // local OTA update from Arduino IDE

#include "emonesp.h"
#include "config.h"
#include "wifi.h"
#include "web_server.h"
#include "ohm.h"
#include "input.h"
#include "emoncms.h"
#include "mqtt.h"
#include "divert.h"

unsigned long Timer1; // Timer for events once every 30 seconds
unsigned long Timer2; // Timer for events once every 1 Minute
unsigned long Timer3; // Timer for events once every 2 seconds

boolean rapi_read = 0; //flag to indicate first read of RAPI status
// -------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------
void
setup() {
  delay(2000);
  Serial.begin(115200);
  pinMode(0, INPUT);

#ifdef DEBUG_SERIAL1
  Serial1.begin(115200);
#endif

  DEBUG.println();
  DEBUG.print("OpenEVSE WiFI ");
  DEBUG.println(ESP.getChipId());
  DEBUG.println("Firmware: " + currentfirmware);

  config_load_settings();
  wifi_setup();
  web_server_setup();
#ifdef ENABLE_OTA
  // Start local OTA update server
  ArduinoOTA.setHostname(esp_hostname);
  ArduinoOTA.begin();
#ifdef WIFI_LED
  ArduinoOTA.onProgress([](unsigned int pos, unsigned int size) {
    DBUGF("Upgrade %d/%d", pos, size);
    static int state = LOW;
    state = !state;
    digitalWrite(WIFI_LED, state);
  });
#endif
#endif
} // end setup

// -------------------------------------------------------------------
// LOOP
// -------------------------------------------------------------------
void
loop() {
  unsigned long loopStart = millis();

  web_server_loop();
  wifi_loop();
#ifdef ENABLE_OTA
  ArduinoOTA.handle();
#endif

  // Gives OpenEVSE time to finish self test on cold start
  if ( (millis() > 5000) && (rapi_read==0) ) {
    DEBUG.println("first read RAPI values");
    handleRapiRead(); //Read all RAPI values
    rapi_read=1;
  }
  // -------------------------------------------------------------------
// Do these things once every 2s
  // -------------------------------------------------------------------
    if ((millis() - Timer3) >= 2000) {
    update_rapi_values();
    Timer3 = millis();
  }

  if (wifi_mode==WIFI_MODE_STA || wifi_mode==WIFI_MODE_AP_AND_STA){

    if (mqtt_server !=0) mqtt_loop();

    // -------------------------------------------------------------------
    // Do these things once every Minute
    // -------------------------------------------------------------------
    if ((millis() - Timer2) >= 60000) {
      DEBUG.println("Time2");
      ohm_loop();
      divert_current_loop();
      Timer2 = millis();
    }
    // -------------------------------------------------------------------
    // Do these things once every 30 seconds
    // -------------------------------------------------------------------
    if ((millis() - Timer1) >= 30000) {
      DEBUG.println("Time1");
      create_rapi_json(); // create JSON Strings for EmonCMS and MQTT
      if (emoncms_apikey != 0)
        emoncms_publish(url);
      if(mqtt_server != 0) mqtt_publish(data);
      Timer1 = millis();
      if (mqtt_server != 0)
        mqtt_publish(data);
    }

  } // end WiFi connected

  unsigned long loopTime = millis() - loopStart;
  if(loopTime > 10) {
    DBUGF("Slow loop %dms", loopTime);
  }
} // end loop
