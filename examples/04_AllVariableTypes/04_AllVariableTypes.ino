// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 04: All Variable Types
// Sensors : temperature (CD_FLOAT), count (CD_INT), presence (CD_BOOL),
//           mode_status (CD_STRING)
// Controls: setpoint (CD_FLOAT, auto-ack), label (CD_STRING, auto-ack)
//
// Numeric types (float/int/bool) map to float slots; string/enum map to status
// slots. Copy each variable's slot from the dashboard's device setup panel.

#if defined(ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#else
#error                                                                         \
    "This example targets ESP32 or ESP8266. The library supports any Arduino-core board with a TLS Client."
#endif
#include <CircuitDigestCloud.h>

// ---- FILL ME IN ------------------------------------------------------------
const char *WIFI_SSID = "your_ssid";
const char *WIFI_PASS = "your_password";
const char *DEVICE_ID = "your-device-id-here";          // Physical Device ID (device setup panel)
const char *CONNECTION_KEY = "your-connection-key"; // Connection Key (device setup panel)
// ---------------------------------------------------------------------------

WiFiClientSecure net;
CircuitDigestCloud cd(net);

float g_setpoint = 25.0f;
char g_label[32] = "default";

void handleSetpoint(const char *var, CDValue v) {
  g_setpoint = v.asFloat();
  Serial.print("setpoint → ");
  Serial.println(g_setpoint);
}

void handleLabel(const char *var, CDValue v) {
  // v.asString() is only valid during this callback — copy if you need it later.
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

  net.setInsecure(); // dev only — pin the Anedya CA for production

  cd.setCredentials(DEVICE_ID, CONNECTION_KEY);
  cd.setDebug(&Serial); // prints debug messages to Serial

  // registerVariable(name, type, slot) — slot from the dashboard.
  cd.registerVariable("temperature", CD_FLOAT, "float0");
  cd.registerVariable("count", CD_INT, "float1");
  cd.registerVariable("presence", CD_BOOL, "float2");
  cd.registerVariable("mode_status", CD_STRING, "status0");

  // onChange(name, cb, ackMode, type, slot)
  cd.onChange("setpoint", handleSetpoint, CD_ACK_AUTO, CD_FLOAT, "float3");
  cd.onChange("label", handleLabel, CD_ACK_AUTO, CD_STRING, "status1");

  cd.begin();
}

void loop() {
  cd.loop();

  static uint32_t last = 0;
  static int count = 0;
  if (millis() - last > 5000) {
    last = millis();

    cd.publishVariable("temperature", 22.5f + count * 0.1f);
    cd.publishVariable("count", count++);
    cd.publishVariable("presence", (count % 2 == 0));
    cd.publishVariable("mode_status", count % 3 == 0 ? "idle" : "active");
  }
}
