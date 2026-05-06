// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 02: Basic Sensor
// Publishes a dummy temperature float to the dashboard every 5 seconds.

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #error "This example targets ESP32 or ESP8266. The library supports any Arduino-core board."
#endif
#include <CircuitDigestCloud.h>

// ---- FILL ME IN ------------------------------------------------------------
// Find these values in your CircuitDigest Cloud dashboard under Device Settings.
const char* WIFI_SSID   = "your_ssid";
const char* WIFI_PASS   = "your_password";
const char* MQTT_DEVICE = "MyDevice";        // MQTT Client ID (any unique name)
const char* MQTT_UUID   = "your-uuid-here";  // User UUID from dashboard
const char* MQTT_DEVID  = "your-devid-here"; // Device ID from dashboard
const char* MQTT_KEY    = "your-key-here";   // Device Key from dashboard
// ---------------------------------------------------------------------------

// Pass any Arduino Client to the constructor — WiFiClient, EthernetClient,
// TinyGsmClient, etc. The library does not manage the connection itself.
WiFiClient net;
CircuitDigestCloud cd(net);

void setup() {
    Serial.begin(115200);
    delay(200);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { delay(200); }

    // setCredentials(device, uuid, devid, key)
    // All four fields are mandatory. Returns false if any is empty.
    cd.setCredentials(MQTT_DEVICE, MQTT_UUID, MQTT_DEVID, MQTT_KEY);

    // setDebug(Stream*)  — pass &Serial to print all library events prefixed [CD].
    cd.setDebug(&Serial);   //comment out to disable debug logging

    // setBufferSize(bytes) — PubSubClient internal buffer. Default: 512. Min: 256.
    // Increase if your variable names or payloads are large.
    // cd.setBufferSize(512);

    // setHeartbeatInterval(seconds) — how often state/last_seen is published.
    // Default: 60. Set to 0 to disable heartbeat entirely.
    // cd.setHeartbeatInterval(60);

    // registerSensor(name, type) — optional pre-registration.
    // Calling publishSensor() without registering first also works (auto-registers).
    // Pre-registering locks the type early and avoids auto-detection overhead.
    //
    // Available types:
    //   CD_AUTO   — type is detected automatically on the first publish
    //   CD_INT    — whole number  (int / long)
    //   CD_FLOAT  — decimal number (float / double)
    //   CD_BOOL   — true / false
    //   CD_STRING — text string   (const char*)
    //   CD_ENUM   — text string treated as an enum (semantic alias for CD_STRING)
    cd.registerSensor("temperature", CD_FLOAT);

    // begin() — initialises the library. Connection to MQTT happens lazily on
    // the first loop() call, not here, so setup() never blocks on the network.
    cd.begin();
}

void loop() {
    // loop() must be called as fast as possible. It drives:
    //   • MQTT connection / reconnection with exponential backoff
    //   • Inbound message dispatch
    //   • Heartbeat scheduler
    // Never put long delay()s before cd.loop().
    cd.loop();

    static uint32_t last = 0;
    if (millis() - last > 5000) {
        last = millis();
        float t = 24.0f + (millis() % 1000) / 1000.0f; // dummy reading

        // publishSensor(name, value)
        // Accepted value types: int, long, float, double, bool, const char*
        // Returns false if not connected or publish failed.
        // Use cd.lastError() immediately after a false return to get the reason.
        cd.publishSensor("temperature", t);
    }
}
