// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 02: Basic Sensor
// Publishes a dummy temperature float to the dashboard every 5 seconds.

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#error                                                                         \
    "This example targets ESP32 or ESP8266. The library supports any Arduino-core board."
#endif
#include <CircuitDigestCloud.h>

// ---- FILL ME IN ------------------------------------------------------------
const char *WIFI_SSID = "your_ssid";
const char *WIFI_PASS = "your_password";
const char *MQTT_USER_ID = "your-uuid-here";    // User UUID from dashboard
const char *MQTT_DEVICE_ID = "your-devid-here"; // Device ID from dashboard
const char *MQTT_KEY = "your-key-here";         // Device Key from dashboard
// ---------------------------------------------------------------------------

// Pass any Arduino Client — WiFiClient, EthernetClient, TinyGsmClient, etc.
WiFiClient net;
CircuitDigestCloud cd(net);

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
  }

  cd.setCredentials(MQTT_USER_ID, MQTT_DEVICE_ID, MQTT_KEY);
  cd.setDebug(&Serial); // prints debug messages to Serial

  // cd.setBufferSize(512);        // PubSubClient buffer (default 512, min 256)
  // cd.setHeartbeatInterval(60);  // heartbeat seconds (default 60, 0 =
  // disabled)

  // Types: CD_AUTO | CD_INT | CD_FLOAT | CD_BOOL | CD_STRING | CD_ENUM
  // Pre-registering is optional — publishVariable() auto-registers on first use.
  cd.registerVariable("temperature", CD_FLOAT);

  cd.begin(); // validates credentials; connection starts on first loop()
}

void loop() {
  cd.loop(); // drives connection, MQTT pump, heartbeat — call every iteration

  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    float t = 24.0f + (millis() % 1000) / 1000.0f; // dummy reading

    // publishVariable(name, value, retain) — retain defaults to true (kept on broker)
    cd.publishVariable("temperature", t);
  }
}
