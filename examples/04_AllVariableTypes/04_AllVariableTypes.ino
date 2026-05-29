// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 04: All Variable Types
// Sensors : temperature (CD_FLOAT), count (CD_INT), presence (CD_BOOL),
// mode_status (CD_STRING) Controls: setpoint (CD_FLOAT, auto-ack), label
// (CD_STRING, auto-ack)

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

float g_setpoint = 25.0f;
char g_label[32] = "default";

void handleSetpoint(const char *var, CDValue v) {
  g_setpoint = v.asFloat();
  Serial.print("setpoint → ");
  Serial.println(g_setpoint);
}

void handleLabel(const char *var, CDValue v) {
  // v.asString() is only valid during this callback — copy if you need it
  // later.
  const char *s = v.asString();
  if (s) {
    strncpy(g_label, s, sizeof(g_label) - 1);
    g_label[sizeof(g_label) - 1] = 0;
  }
  Serial.print("label → ");
  Serial.println(g_label);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
  }

  cd.setCredentials(MQTT_USER_ID, MQTT_DEVICE_ID, MQTT_KEY);
  cd.setDebug(&Serial); // prints debug messages to Serial

  cd.setHeartbeatInterval(30); // heartbeat every 30s (default 60, 0 = disabled)

  // Types: CD_AUTO | CD_INT | CD_FLOAT | CD_BOOL | CD_STRING | CD_ENUM
  cd.registerVariable("temperature", CD_FLOAT);
  cd.registerVariable("count", CD_INT);
  cd.registerVariable("presence", CD_BOOL);
  cd.registerVariable("mode_status", CD_STRING);

  // ackMode: CD_ACK_AUTO (default) | CD_ACK_MANUAL
  cd.onChange("setpoint", handleSetpoint, CD_ACK_AUTO, CD_FLOAT);
  cd.onChange("label", handleLabel, CD_ACK_AUTO, CD_STRING);

  cd.begin();
}

void loop() {
  cd.loop();

  static uint32_t last = 0;
  static int count = 0;
  if (millis() - last > 5000) {
    last = millis();

    // publishSensor(name, value, retain) — retain defaults to true (kept on broker)
    cd.publishSensor("temperature", 22.5f + count * 0.1f);
    cd.publishSensor("count", count++);
    cd.publishSensor("presence", (count % 2 == 0));
    cd.publishSensor("mode_status", count % 3 == 0 ? "idle" : "active");
  }
}
