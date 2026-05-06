// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 05: Manual Ack & Many Controls
// relay_1 and relay_2 use CD_ACK_MANUAL — ack is sent with the actual GPIO read-back.
// mode uses CD_ACK_AUTO. Global fallback handles any unknown controls.

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #error "This example targets ESP32 or ESP8266. The library supports any Arduino-core board."
#endif
#include <CircuitDigestCloud.h>

// ---- FILL ME IN ------------------------------------------------------------
const char* WIFI_SSID   = "your_ssid";
const char* WIFI_PASS   = "your_password";
const char* MQTT_DEVICE = "MyDevice";        // MQTT Client ID (any unique name)
const char* MQTT_UUID   = "your-uuid-here";  // User UUID from dashboard
const char* MQTT_DEVID  = "your-devid-here"; // Device ID from dashboard
const char* MQTT_KEY    = "your-key-here";   // Device Key from dashboard
// ---------------------------------------------------------------------------

#define RELAY1_PIN 26
#define RELAY2_PIN 27

WiFiClient net;
CircuitDigestCloud cd(net);

// CD_ACK_MANUAL example:
// The library does NOT send an ack automatically. You are responsible for calling
// cd.ackControl(name, value) before returning (or any time after).
// Best practice: read back the actual GPIO state and ack with that — so the
// dashboard always reflects reality, not just the requested value.
void handleRelay1(const char* var, CDValue v) {
    digitalWrite(RELAY1_PIN, v.asBool() ? HIGH : LOW);
    bool actual = digitalRead(RELAY1_PIN) == HIGH;

    // ackControl(name, value)
    // Accepted value types: int, long, float, double, bool, const char*
    // Publishes {"relay_1": actual} to control/relay_1/get with retain=true.
    cd.ackControl("relay_1", actual);
}

void handleRelay2(const char* var, CDValue v) {
    digitalWrite(RELAY2_PIN, v.asBool() ? HIGH : LOW);
    bool actual = digitalRead(RELAY2_PIN) == HIGH;
    cd.ackControl("relay_2", actual);
}

// CD_ACK_AUTO example:
// The library acks automatically after this callback returns, echoing the value
// it received. You don't need to call cd.ackControl() yourself.
void handleMode(const char* var, CDValue v) {
    // v.asString() — VALID ONLY DURING THIS CALLBACK.
    // Copy to a local buffer if you need it after this function returns.
    const char* s = v.asString();
    Serial.print("mode → "); Serial.println(s ? s : "(null)");
}

// Global fallback — fires for any inbound control whose name is NOT registered
// with onControl(). Useful for debugging or forwarding unknown commands.
// Note: the library always sends an auto-ack for unknown variables regardless
// of this callback, to prevent the dashboard showing the command as "pending".
void handleUnknown(const char* var, CDValue v) {
    Serial.print("[fallback] unknown control: "); Serial.println(var);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { delay(200); }

    cd.setCredentials(MQTT_DEVICE, MQTT_UUID, MQTT_DEVID, MQTT_KEY);
    
    cd.setDebug(&Serial);   //comment out to disable debug logging

    // onControl(name, callback, ackMode, type)
    //
    // ackMode:
    //   CD_ACK_AUTO   — library acks automatically (default)
    //   CD_ACK_MANUAL — you call cd.ackControl(name, value) yourself
    //
    // type (optional — defaults to CD_AUTO if omitted):
    //   CD_AUTO | CD_INT | CD_FLOAT | CD_BOOL | CD_STRING | CD_ENUM
    cd.onControl("relay_1", handleRelay1, CD_ACK_MANUAL, CD_BOOL);
    cd.onControl("relay_2", handleRelay2, CD_ACK_MANUAL, CD_BOOL);
    cd.onControl("mode",    handleMode,   CD_ACK_AUTO,   CD_STRING);

    // onControl(callback) — global fallback for any unregistered variable name.
    // Only one global fallback is allowed; calling again replaces the previous one.
    // Pass nullptr to clear the fallback.
    cd.onControl(handleUnknown);

    cd.begin();
}

void loop() {
    cd.loop();
}
