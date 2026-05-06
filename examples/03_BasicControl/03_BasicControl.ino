// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 03: Basic Control
// Receives a boolean control "light_1" from the dashboard and drives LED_BUILTIN.

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

// Control callback — called whenever the dashboard sends a command for "light_1".
// Parameters:
//   var  — variable name string (useful when one function handles multiple controls)
//   v    — CDValue holding the received value. Read it with:
//             v.asBool()    — bool
//             v.asInt()     — long
//             v.asFloat()   — float
//             v.asString()  — const char* (valid only during this callback!)
//             v.type()      — CDType: CD_INT | CD_FLOAT | CD_BOOL | CD_STRING | CD_ENUM
void handleLight(const char* var, CDValue v) {
    digitalWrite(LED_BUILTIN, v.asBool() ? HIGH : LOW);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    pinMode(LED_BUILTIN, OUTPUT);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { delay(200); }

    cd.setCredentials(MQTT_DEVICE, MQTT_UUID, MQTT_DEVID, MQTT_KEY);

    cd.setDebug(&Serial);   //comment out to disable debug logging

    // onControl(name, callback, ackMode, type)
    //
    // ackMode — controls how the library confirms the command to the dashboard:
    //   CD_ACK_AUTO   (default) — library sends the ack automatically after your
    //                             callback returns, echoing the received value.
    //   CD_ACK_MANUAL           — library does NOT ack. You must call
    //                             cd.ackControl(name, value) yourself, typically
    //                             after reading back the actual hardware state.
    //
    // type — expected data type from the dashboard (optional, same list as registerSensor):
    //   CD_AUTO   | CD_INT | CD_FLOAT | CD_BOOL | CD_STRING | CD_ENUM
    //   Defaults to CD_AUTO if omitted (type is locked on the first received message).
    cd.onControl("light_1", handleLight);  // CD_ACK_AUTO + CD_AUTO by default

    cd.begin();
}

void loop() {
    cd.loop();
}
