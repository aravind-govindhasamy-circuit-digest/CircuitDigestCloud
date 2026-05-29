// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 05: Manual Ack & Many Controls
// relay_1 and relay_2 use CD_ACK_MANUAL — ack is sent with the actual GPIO
// read-back. mode uses CD_ACK_AUTO. Global fallback handles any unknown
// controls.

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

#define RELAY1_PIN 26
#define RELAY2_PIN 27

WiFiClient net;
CircuitDigestCloud cd(net);

// CD_ACK_MANUAL: you must call cd.ackChange() yourself.
// Best practice: read back actual GPIO state so the dashboard reflects reality.
void handleRelay1(const char *var, CDValue v) {
  digitalWrite(RELAY1_PIN, v.asBool() ? HIGH : LOW);
  bool actual = digitalRead(RELAY1_PIN) == HIGH;
  cd.ackChange("relay_1", actual); // ack with real state
}

void handleRelay2(const char *var, CDValue v) {
  digitalWrite(RELAY2_PIN, v.asBool() ? HIGH : LOW);
  bool actual = digitalRead(RELAY2_PIN) == HIGH;
  cd.ackChange("relay_2", actual);
}

// CD_ACK_AUTO: library acks automatically after callback — no manual ack
// needed.
void handleMode(const char *var, CDValue v) {
  // v.asString() is only valid during this callback — copy if needed.
  const char *s = v.asString();
  Serial.print("mode → ");
  Serial.println(s ? s : "(null)");
}

// Global fallback — fires for any control not registered with onChange().
// Library always auto-acks unknown variables to prevent "pending" state.
void handleUnknown(const char *var, CDValue v) {
  Serial.print("[fallback] unknown control: ");
  Serial.println(var);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
  }

  cd.setCredentials(MQTT_USER_ID, MQTT_DEVICE_ID, MQTT_KEY);
  cd.setDebug(&Serial); // prints debug messages to Serial

  // ackMode: CD_ACK_AUTO (default) | CD_ACK_MANUAL
  // type:    CD_AUTO | CD_INT | CD_FLOAT | CD_BOOL | CD_STRING | CD_ENUM
  cd.onChange("relay_1", handleRelay1, CD_ACK_MANUAL, CD_BOOL);
  cd.onChange("relay_2", handleRelay2, CD_ACK_MANUAL, CD_BOOL);
  cd.onChange("mode", handleMode, CD_ACK_AUTO, CD_STRING);

  // Global fallback — one allowed; pass nullptr to clear.
  cd.onChange(handleUnknown);

  cd.begin();
}

void loop() { cd.loop(); }
