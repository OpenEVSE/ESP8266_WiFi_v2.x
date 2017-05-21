#include "emonesp.h"
#include "mqtt.h"
#include "config.h"
#include "divert.h"

#include <Arduino.h>
#include <PubSubClient.h>             // MQTT https://github.com/knolleary/pubsubclient PlatformIO lib: 89
#include <WiFiClient.h>

WiFiClient espClient;                 // Create client for MQTT
PubSubClient mqttclient(espClient);   // Create client for MQTT

long lastMqttReconnectAttempt = 0;
int clientTimeout = 0;
int i = 0;

// -------------------------------------------------------------------
// MQTT msg Received callback function:
// Function to be called when msg is received on MQTT subscribed topic
// Used to receive RAPI commands via MQTT
// //e.g to set current to 13A: <base-topic>/rapi/$SC 13
// -------------------------------------------------------------------
void mqttmsg_callback(char* topic, byte* payload, unsigned int length)
{

  String topic_string = String(topic);
  
  // print received MQTT to debug
  DEBUG.println("MQTT received:");
  DEBUG.print(topic);
  DEBUG.print(" ");
  for (int i=0;i<length;i++) {
    DEBUG.print((char)payload[i]);
  }
  DEBUG.println();
  
  
  // If MQTT message is solar PV
  if (topic_string = mqtt_solar){
    solar = int(payload);
  }
  
  // If MQTT message is grid import / export
  if (topic_string = mqtt_grid_ie){
    grid_ie = int(payload);
  }
  
  // If MQTT message to set divert mode is received
  if (topic_string = mqtt_topic + "divertmode"){
    divertmode_update(int(payload));
  }
  
  // If MQTT message is RAPI command
  // Detect if MQTT message is a RAPI command e.g to set 13A <base-topic>/rapi/$SC 13
  // Locate '$' character in the MQTT message to identify RAPI command
  int rapi_character_index = topic_string.indexOf('$');
  if (rapi_character_index > 1){
    // Print RAPI command from mqtt-sub topic e.g $SC
    // ASSUME RAPI COMMANDS ARE ALWAYS PREFIX BY $ AND TWO CHARACTERS LONG)
    Serial.flush();
    for (int i=rapi_character_index; i<rapi_character_index+3; i++){
      Serial.print(topic[i]);
    }
    if  (payload[0] != 0);{   // If MQTT msg contains a payload e.g $SC 13. Not all rapi commands have a payload e.g. $GC
      Serial.print(" ");      // print space to seperate RAPI commnd from value
      // print RAPI value received via MQTT serial
      for (int i=0; i<length; i++) {
        Serial.print((char)payload[i]);
      }
    }
    Serial.println(); // End of RAPI command serial print (new line)
    
    // Check RAPI command has been succesful by listing for $OK responce and publish to MQTT under "rapi/out" topic
    delay(60);        // commDelay = 60 (input.cpp)
    while(Serial.available()) {
         String rapiString = Serial.readStringUntil('\r');
         if ( rapiString.startsWith("$OK ") || rapiString.startsWith("$NK ")) {
           String mqtt_data = rapiString;
           String mqtt_sub_topic = mqtt_topic + "/rapi/out";
           mqttclient.publish(mqtt_sub_topic.c_str(), mqtt_data.c_str());
         }
    }
  }

  
} //end call back

// -------------------------------------------------------------------
// MQTT Connect
// -------------------------------------------------------------------
boolean mqtt_connect()
{
  mqttclient.setServer(mqtt_server.c_str(), 1883);
  mqttclient.setCallback(mqttmsg_callback); //function to be called when mqtt msg is received on subscribed topic
  DEBUG.print("MQTT Connecting to...");
  DEBUG.println(mqtt_user.c_str());
  String strID = String(ESP.getChipId());
  if (mqttclient.connect(strID.c_str(), mqtt_user.c_str(), mqtt_pass.c_str())) {  // Attempt to connect
    DEBUG.println("MQTT connected");
    mqttclient.publish(mqtt_topic.c_str(), "connected"); // Once connected, publish an announcement..
    String mqtt_sub_topic = mqtt_topic + "/rapi/in/#";      // MQTT Topic to subscribe to receive RAPI commands via MQTT
    //e.g to set current to 13A: <base-topic>/rapi/in/$SC 13
    mqttclient.subscribe(mqtt_sub_topic.c_str());
    // subscribe to solar PV / grid_ie MQTT feeds
    if (mqtt_solar!=""){
      mqttclient.subscribe(mqtt_solar.c_str());
    }
    if (mqtt_grid_ie!=""){
      mqttclient.subscribe(mqtt_grid_ie.c_str());
    }
    mqtt_sub_topic = mqtt_topic + "divertmode";      // MQTT Topic to change divert mode
    mqttclient.subscribe(mqtt_sub_topic.c_str());
    
  } else {
    DEBUG.print("MQTT failed: ");
    DEBUG.println(mqttclient.state());
    return(0);
  }
  return (1);
}



// -------------------------------------------------------------------
// Publish status to MQTT
// -------------------------------------------------------------------
void mqtt_publish(String data)
{
  String mqtt_data = "";
  String topic = mqtt_topic + "/";
  
  int i=0;
  while (int(data[i])!=0)
  {
    // Construct MQTT topic e.g. <base_topic>/<status> data
    while (data[i]!=':'){
      topic+= data[i];
      i++;
      if (int(data[i])==0){
        break;
      }
    }
    i++;
    // Construct data string to publish to above topic
    while (data[i]!=','){
      mqtt_data+= data[i];
      i++;
      if (int(data[i])==0){
        break;
      }
    }
    // send data via mqtt
    //delay(100);
    DEBUG.printf("%s = %s\r\n", topic.c_str(), mqtt_data.c_str());
    mqttclient.publish(topic.c_str(), mqtt_data.c_str());
    topic = mqtt_topic + "/";
    mqtt_data="";
    i++;
    if (int(data[i])==0) break;
  }
  
  // Publish free RAM (heap) to <base-topic>/freeram
  String str_topic = topic + "freeram";
  String str_msg = String(ESP.getFreeHeap());
  mqttclient.publish(str_topic.c_str(), str_msg.c_str());

  // Publish divertmode to <base-topic>/divertmode
  str_topic = topic + "divertmode";
  str_msg = String(divertmode);
  mqttclient.publish(str_topic.c_str(), str_msg.c_str());
}

// -------------------------------------------------------------------
// MQTT state management
//
// Call every time around loop() if connected to the WiFi
// -------------------------------------------------------------------
void mqtt_loop()
{
  if (!mqttclient.connected()) {
    long now = millis();
    // try and reconnect continuously for first 5s then try again once every 10s
    if ( (now < 50000) || ((now - lastMqttReconnectAttempt)  > 100000) ) {
      lastMqttReconnectAttempt = now;
      if (mqtt_connect()) { // Attempt to reconnect
        lastMqttReconnectAttempt = 0;
      }
    }
  } else {
    // if MQTT connected
    mqttclient.loop();
  }
}

void mqtt_restart()
{
  if (mqttclient.connected()) {
    mqttclient.disconnect();
  }
}

boolean mqtt_connected()
{
  return mqttclient.connected();
}
