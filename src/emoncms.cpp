#if defined(ENABLE_DEBUG) && !defined(ENABLE_DEBUG_EMONCMS)
#undef ENABLE_DEBUG
#endif

#include <Arduino.h>
#include <ArduinoJson.h>

#include "emonesp.h"
#include "emoncms.h"
#include "app_config.h"
#include "http.h"
#include "input.h"
#include "event.h"
#include "urlencode.h"

boolean emoncms_connected = false;
boolean emoncms_updated = false;

unsigned long packets_sent = 0;
unsigned long packets_success = 0;

const char *post_path = "/input/post?";

static void emoncms_result(bool success, String message)
{
  StaticJsonDocument<128> event;
  
  emoncms_connected = success;
  event["emoncms_connected"] = (int)emoncms_connected;
  event["emoncms_message"] = message.substring(0, 64);
  event_send(event);
}

void emoncms_publish(JsonDocument &data)
{
  Profile_Start(emoncms_publish);

  if (config_emoncms_enabled() && emoncms_apikey != 0)
  {
    String url = post_path;
    String json;
    serializeJson(data, json);
    url += "fulljson=";
    url += urlencode(json);
    url += "&node=";
    url += emoncms_node;
    url += "&apikey=";
    url += emoncms_apikey;

    DBUGVAR(url);

    DEBUG.println(emoncms_server.c_str() + String(url));
    packets_sent++;
    // Send data to Emoncms server
    String result = "";
    if (emoncms_fingerprint != 0) {
      // HTTPS on port 443 if HTTPS fingerprint is present
      DEBUG.println("HTTPS");
      delay(10);
      result =
        get_https(emoncms_fingerprint.c_str(), emoncms_server.c_str(), url,
                  443);
    } else {
      // Plain HTTP if other emoncms server e.g EmonPi
      DEBUG.println("HTTP");
      delay(10);
      result = get_http(emoncms_server.c_str(), url);
    }

    const size_t capacity = JSON_OBJECT_SIZE(2) + result.length();
    DynamicJsonDocument doc(capacity);
    if(DeserializationError::Code::Ok == deserializeJson(doc, result.c_str(), result.length()))
    {
      DBUGLN("Got JSON");
      bool success = doc["success"]; // true
      if(success) {
        packets_success++;
      }
      emoncms_result(success, doc["message"]);
    } else if (result == "ok") {
      packets_success++;
      emoncms_result(true, result);
    } else {
      DEBUG.print("Emoncms error: ");
      DEBUG.println(result);
      emoncms_result(false, result);
    }
  } else {
    if(emoncms_connected) {
      emoncms_result(false, String("Disabled"));
    }
  }

  Profile_End(emoncms_publish, 10);
}
