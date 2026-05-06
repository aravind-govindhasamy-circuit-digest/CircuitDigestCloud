// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 04: All Variable Types
// Sensors : temperature (CD_FLOAT), count (CD_INT), presence (CD_BOOL), mode_status (CD_STRING)
// Controls: setpoint (CD_FLOAT, auto-ack), label (CD_STRING, auto-ack)

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

WiFiClient net;
CircuitDigestCloud cd(net);

float g_setpoint = 25.0f;
char  g_label[32] = "default";

void handleSetpoint(const char* var, CDValue v) {
    // v.asFloat()  — returns float. Cross-type rules:
    //   CD_INT   → promoted to float exactly
    //   CD_FLOAT → returned as-is
    //   CD_BOOL  → 1.0 or 0.0
    g_setpoint = v.asFloat();
    Serial.print("setpoint → "); Serial.println(g_setpoint);
}

void handleLabel(const char* var, CDValue v) {
    // IMPORTANT: v.asString() points into an internal buffer that is overwritten
    // on the next inbound message. Copy it immediately if you need it to persist.
    const char* s = v.asString();
    if (s) {
        strncpy(g_label, s, sizeof(g_label) - 1);
        g_label[sizeof(g_label) - 1] = 0;
    }
    Serial.print("label → "); Serial.println(g_label);
}

void setup() {
    Serial.begin(115200);
    delay(200);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { delay(200); }

    cd.setCredentials(MQTT_DEVICE, MQTT_UUID, MQTT_DEVID, MQTT_KEY);
    
    cd.setDebug(&Serial);   //comment out to disable debug logging

    // setHeartbeatInterval(seconds)
    //   Default: 60  — publishes an empty keep-alive to state/last_seen every N seconds.
    //   Set to 0     — disables heartbeat entirely.
    cd.setHeartbeatInterval(30);

    // registerSensor(name, type)
    // Available types:
    //   CD_AUTO   — auto-detected on first publish (default)
    //   CD_INT    — whole number  → publishSensor accepts int or long
    //   CD_FLOAT  — decimal      → publishSensor accepts float or double
    //   CD_BOOL   — true/false   → publishSensor accepts bool
    //   CD_STRING — text         → publishSensor accepts const char*
    //   CD_ENUM   — text enum    → wire-identical to CD_STRING; semantic hint only
    cd.registerSensor("temperature",  CD_FLOAT);
    cd.registerSensor("count",        CD_INT);
    cd.registerSensor("presence",     CD_BOOL);
    cd.registerSensor("mode_status",  CD_STRING);  // could also use CD_ENUM

    // onControl(name, callback, ackMode, type)
    // ackMode options:
    //   CD_ACK_AUTO   — library acks automatically after callback (default)
    //   CD_ACK_MANUAL — you must call cd.ackControl(name, value) yourself
    // type options: same list as registerSensor above
    cd.onControl("setpoint", handleSetpoint, CD_ACK_AUTO, CD_FLOAT);
    cd.onControl("label",    handleLabel,    CD_ACK_AUTO, CD_STRING);

    cd.begin();
}

void loop() {
    cd.loop();

    static uint32_t last = 0;
    static int count = 0;
    if (millis() - last > 5000) {
        last = millis();

        // publishSensor(name, value)
        // Accepted C++ types: int, long, float, double, bool, const char*
        // The library formats the value as JSON: {"name": value}
        cd.publishSensor("temperature", 22.5f + count * 0.1f); // float
        cd.publishSensor("count",       count++);               // int
        cd.publishSensor("presence",    (count % 2 == 0));      // bool
        cd.publishSensor("mode_status", count % 3 == 0 ? "idle" : "active"); // string
    }
}
