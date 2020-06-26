#include "emonesp.h"
#include "app_config.h"
#include "espal.h"

#include <Arduino.h>
#include <EEPROM.h>             // Save config settings

#define EEPROM_ESID_SIZE              32
#define EEPROM_EPASS_SIZE             64
#define EEPROM_EMON_API_KEY_SIZE      33
#define EEPROM_EMON_SERVER_SIZE       45
#define EEPROM_EMON_NODE_SIZE         32
#define EEPROM_MQTT_SERVER_SIZE       45
#define EEPROM_MQTT_TOPIC_SIZE        32
#define EEPROM_MQTT_USER_SIZE         32
#define EEPROM_MQTT_PASS_SIZE         64
#define EEPROM_MQTT_SOLAR_SIZE        30
#define EEPROM_MQTT_GRID_IE_SIZE      30
#define EEPROM_EMON_FINGERPRINT_SIZE  60
#define EEPROM_WWW_USER_SIZE          15
#define EEPROM_WWW_PASS_SIZE          15
#define EEPROM_OHM_KEY_SIZE           10
#define EEPROM_FLAGS_SIZE             4
#define EEPROM_HOSTNAME_SIZE          32
#define EEPROM_SIZE                   1024

#define EEPROM_ESID_START             0
#define EEPROM_ESID_END               (EEPROM_ESID_START + EEPROM_ESID_SIZE)
#define EEPROM_EPASS_START            EEPROM_ESID_END
#define EEPROM_EPASS_END              (EEPROM_EPASS_START + EEPROM_EPASS_SIZE)
#define EEPROM_EMON_SERVER_START      EEPROM_EPASS_END + 32 /* EEPROM_EMON_API_KEY used to be stored before this */
#define EEPROM_EMON_SERVER_END        (EEPROM_EMON_SERVER_START + EEPROM_EMON_SERVER_SIZE)
#define EEPROM_EMON_NODE_START        EEPROM_EMON_SERVER_END
#define EEPROM_EMON_NODE_END          (EEPROM_EMON_NODE_START + EEPROM_EMON_NODE_SIZE)
#define EEPROM_MQTT_SERVER_START      EEPROM_EMON_NODE_END
#define EEPROM_MQTT_SERVER_END        (EEPROM_MQTT_SERVER_START + EEPROM_MQTT_SERVER_SIZE)
#define EEPROM_MQTT_TOPIC_START       EEPROM_MQTT_SERVER_END
#define EEPROM_MQTT_TOPIC_END         (EEPROM_MQTT_TOPIC_START + EEPROM_MQTT_TOPIC_SIZE)
#define EEPROM_MQTT_USER_START        EEPROM_MQTT_TOPIC_END
#define EEPROM_MQTT_USER_END          (EEPROM_MQTT_USER_START + EEPROM_MQTT_USER_SIZE)
#define EEPROM_MQTT_PASS_START        EEPROM_MQTT_USER_END
#define EEPROM_MQTT_PASS_END          (EEPROM_MQTT_PASS_START + EEPROM_MQTT_PASS_SIZE)
#define EEPROM_MQTT_SOLAR_START       EEPROM_MQTT_PASS_END
#define EEPROM_MQTT_SOLAR_END         (EEPROM_MQTT_SOLAR_START + EEPROM_MQTT_SOLAR_SIZE)
#define EEPROM_MQTT_GRID_IE_START     EEPROM_MQTT_SOLAR_END
#define EEPROM_MQTT_GRID_IE_END       (EEPROM_MQTT_GRID_IE_START + EEPROM_MQTT_GRID_IE_SIZE)
#define EEPROM_EMON_FINGERPRINT_START EEPROM_MQTT_GRID_IE_END
#define EEPROM_EMON_FINGERPRINT_END   (EEPROM_EMON_FINGERPRINT_START + EEPROM_EMON_FINGERPRINT_SIZE)
#define EEPROM_WWW_USER_START         EEPROM_EMON_FINGERPRINT_END
#define EEPROM_WWW_USER_END           (EEPROM_WWW_USER_START + EEPROM_WWW_USER_SIZE)
#define EEPROM_WWW_PASS_START         EEPROM_WWW_USER_END
#define EEPROM_WWW_PASS_END           (EEPROM_WWW_PASS_START + EEPROM_WWW_PASS_SIZE)
#define EEPROM_OHM_KEY_START          EEPROM_WWW_PASS_END
#define EEPROM_OHM_KEY_END            (EEPROM_OHM_KEY_START + EEPROM_OHM_KEY_SIZE)
#define EEPROM_FLAGS_START            EEPROM_OHM_KEY_END
#define EEPROM_FLAGS_END              (EEPROM_FLAGS_START + EEPROM_FLAGS_SIZE)
#define EEPROM_EMON_API_KEY_START     EEPROM_FLAGS_END
#define EEPROM_EMON_API_KEY_END       (EEPROM_EMON_API_KEY_START + EEPROM_EMON_API_KEY_SIZE)
#define EEPROM_HOSTNAME_START         EEPROM_EMON_API_KEY_END
#define EEPROM_HOSTNAME_END           (EEPROM_HOSTNAME_START + EEPROM_HOSTNAME_SIZE)
#define EEPROM_CONFIG_END             EEPROM_HOSTNAME_END

#if EEPROM_CONFIG_END > EEPROM_SIZE
#error EEPROM_SIZE too small
#endif

#define CHECKSUM_SEED 128

bool
EEPROM_read_string(int start, int count, String & val) {
  String newVal;
  byte checksum = CHECKSUM_SEED;
  for (int i = 0; i < count - 1; ++i) {
    byte c = EEPROM.read(start + i);
    if (c != 0 && c != 255) {
      checksum ^= c;
      newVal += (char) c;
    } else {
      break;
    }
  }

  // Check the checksum
  byte c = EEPROM.read(start + (count - 1));
  DBUGF("Got '%s' %d == %d @ %d:%d", newVal.c_str(), c, checksum, start, count);
  if(c == checksum) {
    val = newVal;
    return true;
  }

  return false;
}

void
EEPROM_read_uint24(int start, uint32_t & val) {
  byte checksum = CHECKSUM_SEED;
  uint32_t newVal = 0;
  for (int i = 0; i < 3; ++i) {
    byte c = EEPROM.read(start + i);
    checksum ^= c;
    newVal = (newVal << 8) | c;
  }

  // Check the checksum
  byte c = EEPROM.read(start + 3);
  DBUGF("Got '%06x'  %d == %d @ %d:4", newVal, c, checksum, start);
  if(c == checksum) {
    val = newVal;
  }
}

// -------------------------------------------------------------------
// Load saved settings from EEPROM
// -------------------------------------------------------------------
void
config_load_v1_settings() {
  DBUGLN("Loading config");

  EEPROM.begin(EEPROM_SIZE);

  // Device Hostname, needs to be read first as other config defaults depend on it
  EEPROM_read_string(EEPROM_HOSTNAME_START, EEPROM_HOSTNAME_SIZE,
                     esp_hostname);

  // Load WiFi values
  EEPROM_read_string(EEPROM_ESID_START, EEPROM_ESID_SIZE, esid);
  EEPROM_read_string(EEPROM_EPASS_START, EEPROM_EPASS_SIZE, epass);

  // EmonCMS settings
  EEPROM_read_string(EEPROM_EMON_API_KEY_START, EEPROM_EMON_API_KEY_SIZE,
                     emoncms_apikey);
  EEPROM_read_string(EEPROM_EMON_SERVER_START, EEPROM_EMON_SERVER_SIZE,
                     emoncms_server);
  EEPROM_read_string(EEPROM_EMON_NODE_START, EEPROM_EMON_NODE_SIZE,
                     emoncms_node);
  EEPROM_read_string(EEPROM_EMON_FINGERPRINT_START, EEPROM_EMON_FINGERPRINT_SIZE,
                     emoncms_fingerprint);

  // MQTT settings
  EEPROM_read_string(EEPROM_MQTT_SERVER_START, EEPROM_MQTT_SERVER_SIZE,
                     mqtt_server);
  EEPROM_read_string(EEPROM_MQTT_TOPIC_START, EEPROM_MQTT_TOPIC_SIZE,
                     mqtt_topic);
  EEPROM_read_string(EEPROM_MQTT_USER_START, EEPROM_MQTT_USER_SIZE,
                     mqtt_user);
  EEPROM_read_string(EEPROM_MQTT_PASS_START, EEPROM_MQTT_PASS_SIZE,
                     mqtt_pass);
  EEPROM_read_string(EEPROM_MQTT_SOLAR_START, EEPROM_MQTT_SOLAR_SIZE,
                     mqtt_solar);
  EEPROM_read_string(EEPROM_MQTT_GRID_IE_START, EEPROM_MQTT_GRID_IE_SIZE,
                     mqtt_grid_ie);

  // Web server credentials
  EEPROM_read_string(EEPROM_WWW_USER_START, EEPROM_WWW_USER_SIZE,
                     www_username);
  EEPROM_read_string(EEPROM_WWW_PASS_START, EEPROM_WWW_PASS_SIZE,
                     www_password);
  // Web server credentials
  EEPROM_read_string(EEPROM_HOSTNAME_START, EEPROM_HOSTNAME_SIZE,
                     esp_hostname);

  // Ohm Connect Settings
  EEPROM_read_string(EEPROM_OHM_KEY_START, EEPROM_OHM_KEY_SIZE, ohm);

  // Flags
  EEPROM_read_uint24(EEPROM_FLAGS_START, flags);

  EEPROM.end();
}
