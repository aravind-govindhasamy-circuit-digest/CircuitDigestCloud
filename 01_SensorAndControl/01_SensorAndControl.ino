// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 01: Sensor + GPIO Control
// Publishes a dummy temperature sensor every 5 seconds.
// Receives a boolean control "gpio" to drive GPIO 8.

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

#define GPIO_PIN 8

WiFiClient net;
CircuitDigestCloud cd(net);

// Control callback for "gpio".
// The dashboard sends true (HIGH) or false (LOW).
// CDValue methods available inside any callback:
//   v.asBool()   — bool        (true/false)
//   v.asInt()    — long        (0 or 1 for booleans)
//   v.asFloat()  — float       (0.0 or 1.0 for booleans)
//   v.asString() — const char* (nullptr for non-string types; valid this callback only)
//   v.type()     — CDType: CD_INT | CD_FLOAT | CD_BOOL | CD_STRING | CD_ENUM | CD_AUTO
void handleGpio(const char* var, CDValue v) {
    bool state = v.asBool();
    digitalWrite(GPIO_PIN, state ? HIGH : LOW);
    Serial.print("GPIO → "); Serial.println(state ? "ON" : "OFF");
    // CD_ACK_AUTO is set below, so the library sends the ack automatically
    // after this callback returns — no need to call cd.ackControl() here.
}

void setup() {
    Serial.begin(115200);
    delay(200);
    pinMode(GPIO_PIN, OUTPUT);
    digitalWrite(GPIO_PIN, LOW);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) { delay(200); Serial.print("."); }
    Serial.println(" connected");

    // setCredentials(device, uuid, devid, key)
    // All four fields mandatory. Returns false if any is empty.
    cd.setCredentials(MQTT_DEVICE, MQTT_UUID, MQTT_DEVID, MQTT_KEY);

    cd.setDebug(&Serial);   //comment out to disable debug logging

    // registerSensor(name, type)
    // Pre-registers the variable so its type is locked before the first publish.
    // Available types:
    //   CD_AUTO   — auto-detect on first publish (default)
    //   CD_INT    — whole number  (int / long)
    //   CD_FLOAT  — decimal      (float / double)
    //   CD_BOOL   — true / false
    //   CD_STRING — text         (const char*)
    //   CD_ENUM   — text enum    (wire-identical to CD_STRING)
    cd.registerSensor("temperature", CD_FLOAT);

    // onControl(name, callback, ackMode, type)
    // ackMode options:
    //   CD_ACK_AUTO   — library acks automatically after callback returns (default)
    //   CD_ACK_MANUAL — you must call cd.ackControl(name, value) yourself
    // type options: same list as registerSensor above
    cd.onControl("gpio", handleGpio, CD_ACK_AUTO, CD_BOOL);

    // begin() — validates credentials and caches MQTT topics.
    // Actual network connection happens on the first loop() call.
    cd.begin();
}

void loop() {
    // Must be called every loop iteration. Handles connection, MQTT pump,
    // inbound dispatch, and heartbeat. Never put long delay()s before this.
    cd.loop();

    static uint32_t last = 0;
    if (millis() - last > 5000) {
        last = millis();

        // Dummy temperature stepping between 20.0 and 30.0 °C
        static float temp = 20.0f;
        temp += 0.5f;
        if (temp > 30.0f) temp = 20.0f;

        // publishSensor(name, value)
        // Accepted C++ types: int, long, float, double, bool, const char*
        // Returns false if disconnected or publish failed.
        // Check cd.lastError() for the reason on failure.
        cd.publishSensor("temperature", temp);
    }
}
