// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 05: Manual Ack & Many Controls
// relay_1 and relay_2 use CD_ACK_MANUAL — ack is sent with the actual GPIO
// read-back. mode uses CD_ACK_AUTO. Global fallback handles any unknown controls.

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

#define RELAY1_PIN 26
#define RELAY2_PIN 27

WiFiClientSecure net;
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

// CD_ACK_AUTO: library reports the value automatically after callback.
void handleMode(const char *var, CDValue v) {
  // v.asString() is only valid during this callback — copy if needed.
  const char *s = v.asString();
  Serial.print("mode → ");
  Serial.println(s ? s : "(null)");
}

// Global fallback — fires for any control not registered with onChange().
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

  net.setInsecure(); // dev only — pin the Anedya CA for production

  cd.setCredentials(DEVICE_ID, CONNECTION_KEY);
  cd.setDebug(&Serial); // prints debug messages to Serial

  // onChange(name, cb, ackMode, type, slot) — slots from the dashboard.
  cd.onChange("relay_1", handleRelay1, CD_ACK_MANUAL, CD_BOOL, "float0");
  cd.onChange("relay_2", handleRelay2, CD_ACK_MANUAL, CD_BOOL, "float1");
  cd.onChange("mode", handleMode, CD_ACK_AUTO, CD_STRING, "status0");

  // Global fallback — one allowed; pass nullptr to clear.
  cd.onChange(handleUnknown);

  cd.begin();
}

void loop() { cd.loop(); }
