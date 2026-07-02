// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 04: Publish multiple variables in one call
//
// Sends temperature, humidity and pressure every 5 seconds as a single
// MQTT message using one publish() call with a {key, value} list.
// Works on: ESP32, ESP8266, Arduino UNO R4 WiFi, Raspberry Pi Pico W / Pico 2 W.
// No WiFi or TLS setup needed — the library handles everything.

#include <CircuitDigestCloud.h>

// ── Fill in your credentials ────────────────────────────────────────────────
#define WIFI_SSID      "your_ssid"
#define WIFI_PASS      "your_password"
#define DEVICE_ID      "your-device-id"        // Physical Device ID (dashboard)
#define CONNECTION_KEY "your-connection-key"    // Connection Key    (dashboard)
#define API_KEY        "cd_live_xxxxxxxxxxxx"   // API Key           (dashboard)

// Variable keys shown next to your variables on the dashboard
#define MyTemperature  "temperature-1"
#define MyHumidity     "humidity-1"
#define MyPressure     "pressure-1"
// ────────────────────────────────────────────────────────────────────────────

CircuitDigestCloud CDcloud;

void setup() {
  Serial.begin(115200);
  // begin() connects WiFi then starts the MQTT state machine.
  if (!CDcloud.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, API_KEY)) {
    Serial.println("begin() failed — check credentials");
    while (true) delay(1000);
  }
}

void loop() {
  CDcloud.loop();   // drives WiFi reconnect + MQTT + auto heartbeat

  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    float temperature = random(200,  300)  / 10.0f;   // replace with real sensors
    float humidity    = random(400,  700)  / 10.0f;
    float pressure    = random(9800, 10300) / 10.0f;

    // One call, one MQTT message — any number of {key, value} pairs.
    // The payload must fit CD_SUBMIT_BUFFER_SIZE (default 512 B ≈ 8 readings).
    CDcloud.publish({{MyTemperature, temperature},
                     {MyHumidity,    humidity},
                     {MyPressure,    pressure}});
  }
}
