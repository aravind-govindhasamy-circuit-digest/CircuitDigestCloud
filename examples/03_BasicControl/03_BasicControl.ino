// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 03: Basic Control
// Receives a boolean control "light_1" from the dashboard and drives
// LED_BUILTIN.

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

WiFiClient net;
CircuitDigestCloud cd(net);

// Control callback — v.asBool() / v.asInt() / v.asFloat() / v.asString() /
// v.type()
void handleLight(const char *var, CDValue v) {
  digitalWrite(LED_BUILTIN, v.asBool() ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(LED_BUILTIN, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
  }

  cd.setCredentials(MQTT_USER_ID, MQTT_DEVICE_ID, MQTT_KEY);
  cd.setDebug(&Serial); // prints debug messages to Serial

  // ackMode: CD_ACK_AUTO (default) | CD_ACK_MANUAL (you call cd.ackControl())
  // type:    CD_AUTO | CD_INT | CD_FLOAT | CD_BOOL | CD_STRING | CD_ENUM
  cd.onChange("light_1", handleLight); // CD_ACK_AUTO + CD_AUTO by default

  cd.begin();
}

void loop() { cd.loop(); }
