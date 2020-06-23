#include "emonesp.h"
#include "mqtt.h"
#include "app_config.h"
#include "divert.h"
#include "input.h"
#include "espal.h"

#include "openevse.h"

#include <Arduino.h>
#include <PubSubClient.h>             // MQTT https://github.com/knolleary/pubsubclient PlatformIO lib: 89
#include <WiFiClient.h>

WiFiClient espClient;                 // Create client for MQTT
PubSubClient mqttclient(espClient);   // Create client for MQTT

static long nextMqttReconnectAttempt = 0;
static unsigned long mqttRestartTime = 0;

int clientTimeout = 0;
int i = 0;

#ifndef MQTT_CONNECT_TIMEOUT
#define MQTT_CONNECT_TIMEOUT (5 * 1000)
#endif // !MQTT_CONNECT_TIMEOUT

// -------------------------------------------------------------------
// MQTT msg Received callback function:
// Function to be called when msg is received on MQTT subscribed topic
// Used to receive RAPI commands via MQTT
// //e.g to set current to 13A: <base-topic>/rapi/$SC 13
// -------------------------------------------------------------------
void mqttmsg_callback(char *topic, byte * payload, unsigned int length) {

  String topic_string = String(topic);
  String payload_str = "";
  // print received MQTT to debug

  DBUGLN("MQTT received:");
  DBUGF("Topic: %s", topic);

  for (int i = 0; i < length; i++) {
    payload_str += (char) payload[i];
  }
  DEBUG.println("Payload: " + payload_str);

  // If MQTT message is solar PV
  if (topic_string == mqtt_solar){
    solar = payload_str.toInt();
    DBUGF("solar:%dW", solar);
    divert_update_state();
  }
  else if (topic_string == mqtt_grid_ie){
    grid_ie = payload_str.toInt();
    DBUGF("grid:%dW", grid_ie);
    divert_update_state();
  }
  else if (topic_string == mqtt_vrms){
    voltage = payload_str.toFloat();
    DBUGF("voltage: %.2f", voltage);
    OpenEVSE.setVoltage(voltage, [](int ret) {
      // Only gives better power calculations so not critical if this fails
    });
  }
  // If MQTT message to set divert mode is received
  else if (topic_string == mqtt_topic + "/divertmode/set"){
    byte newdivert = payload_str.toInt();
    if ((newdivert==1) || (newdivert==2)){
      divertmode_update(newdivert);
    }
  }
  else
  {
    // If MQTT message is RAPI command
    // Detect if MQTT message is a RAPI command e.g to set 13A <base-topic>/rapi/$SC 13
    // Locate '$' character in the MQTT message to identify RAPI command
    int rapi_character_index = topic_string.indexOf('$');
    DBUGVAR(rapi_character_index);
    if (rapi_character_index > 1) {
      DBUGF("Processing as RAPI");
      // Print RAPI command from mqtt-sub topic e.g $SC
      // ASSUME RAPI COMMANDS ARE ALWAYS PREFIX BY $ AND TWO CHARACTERS LONG)
      String cmd = String(topic + rapi_character_index);
      if (payload[0] != 0); {     // If MQTT msg contains a payload e.g $SC 13. Not all rapi commands have a payload e.g. $GC
        cmd += " ";
        // print RAPI value received via MQTT serial
        for (unsigned int i = 0; i < length; i++) {
          cmd += (char)payload[i];
        }
      }

      rapiSender.sendCmd(cmd, [](int ret)
      {
        if (RAPI_RESPONSE_OK == ret || RAPI_RESPONSE_NK == ret)
        {
          String rapiString = rapiSender.getResponse();
          String mqtt_data = rapiString;
          String mqtt_sub_topic = mqtt_topic + "/rapi/out";
          mqttclient.publish(mqtt_sub_topic.c_str(), mqtt_data.c_str());
        }
      });
    }
  }
} //end call back

// -------------------------------------------------------------------
// MQTT Connect
// -------------------------------------------------------------------
boolean
mqtt_connect() {
  mqttclient.setServer(mqtt_server.c_str(), mqtt_port);
  mqttclient.setCallback(mqttmsg_callback); //function to be called when mqtt msg is received on subscribed topic
  DEBUG.print("MQTT Connecting to...");
  DEBUG.println(mqtt_user.c_str());
  String strID = String(ESP.getChipId());
  if (mqttclient.connect(strID.c_str(), mqtt_user.c_str(), mqtt_pass.c_str(),mqtt_topic.c_str(),1,0,(char*)"disconnected")) {  // Attempt to connect
    DEBUG.println("MQTT connected");
    mqttclient.publish(mqtt_topic.c_str(), "connected"); // Once connected, publish an announcement..
    String mqtt_sub_topic = mqtt_topic + "/rapi/in/#";      // MQTT Topic to subscribe to receive RAPI commands via MQTT
    //e.g to set current to 13A: <base-topic>/rapi/in/$SC 13
    mqttclient.subscribe(mqtt_sub_topic.c_str());
    // subscribe to solar PV / grid_ie MQTT feeds
    if(config_divert_enabled())
    {
      if (mqtt_solar!="") {
        mqttclient.subscribe(mqtt_solar.c_str());
      }
      if (mqtt_grid_ie!="") {
        mqttclient.subscribe(mqtt_grid_ie.c_str());
      }
    }
    if (mqtt_vrms!="") {
      mqttclient.subscribe(mqtt_vrms.c_str());
    }

    mqtt_sub_topic = mqtt_topic + "/divertmode/set";      // MQTT Topic to change divert mode
    mqttclient.subscribe(mqtt_sub_topic.c_str());

  } else {
    DEBUG.print("MQTT failed: ");
    DEBUG.println(mqttclient.state());
    return (0);
  }
  return (1);
}



// -------------------------------------------------------------------
// Publish status to MQTT
// -------------------------------------------------------------------
void
mqtt_publish(JsonDocument &data) {
  Profile_Start(mqtt_publish);

  if(!config_mqtt_enabled() || !mqttclient.connected()) {
    return;
  }

  JsonObject root = data.as<JsonObject>();
  for (JsonPair kv : root) {
    String topic = mqtt_topic + "/";
    topic += kv.key().c_str();
    String val = kv.value().as<String>();
    mqttclient.publish(topic.c_str(), val.c_str());
  }

  Profile_End(mqtt_publish, 5);
}

// -------------------------------------------------------------------
// MQTT state management
//
// Call every time around loop() if connected to the WiFi
// -------------------------------------------------------------------
void
mqtt_loop() {
  Profile_Start(mqtt_loop);

  // Do we need to restart MQTT?
  if(mqttRestartTime > 0 && millis() > mqttRestartTime) 
  {
    mqttRestartTime = 0;
    if (mqttclient.connected()) {
      DBUGF("Disconnecting MQTT");
      mqttclient.disconnect();
    }
    nextMqttReconnectAttempt = 0;
  }

  if(config_mqtt_enabled())
  {
    if (!mqttclient.connected()) {
      long now = millis();
      // try and reconnect every x seconds
      if (now > nextMqttReconnectAttempt) {
        nextMqttReconnectAttempt = now + MQTT_CONNECT_TIMEOUT;
        mqtt_connect(); // Attempt to reconnect
      }
    } else {
      // if MQTT connected
      mqttclient.loop();
    }
  }

  Profile_End(mqtt_loop, 5);
}

void
mqtt_restart() {
  // If connected disconnect MQTT to trigger re-connect with new details
  mqttRestartTime = millis();
}

boolean
mqtt_connected() {
  return mqttclient.connected();
}
