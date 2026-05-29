// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 01: Sensor + GPIO Control
// Publishes a dummy temperature sensor every 5 seconds.
// Receives a boolean control "gpio" to drive GPIO 2.

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

#define GPIO_PIN 2

WiFiClient net;
CircuitDigestCloud cd(net);

// Control callback — v.asBool() / v.asInt() / v.asFloat() / v.asString() /
// v.type()
void handleGpio(const char *var, CDValue v) {
  bool state = v.asBool();
  digitalWrite(GPIO_PIN, state ? HIGH : LOW);
  Serial.print("GPIO → ");
  Serial.println(state ? "ON" : "OFF");
  // CD_ACK_AUTO echoes the value back automatically — no manual ack needed.
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(GPIO_PIN, OUTPUT);
  digitalWrite(GPIO_PIN, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println(" connected");

  cd.setCredentials(MQTT_USER_ID, MQTT_DEVICE_ID, MQTT_KEY);
  cd.setDebug(&Serial); // prints debug messages to Serial

  // Types: CD_AUTO (default) | CD_INT | CD_FLOAT | CD_BOOL | CD_STRING |
  // CD_ENUM
  cd.registerVariable("temperature", CD_FLOAT);

  // ackMode: CD_ACK_AUTO (default) | CD_ACK_MANUAL (you call cd.ackControl())
  cd.onChange("gpio", handleGpio, CD_ACK_AUTO, CD_BOOL);

  cd.begin(); // validates credentials; connection starts on first loop()
}

void loop() {
  cd.loop(); // drives connection, MQTT pump, heartbeat — call every iteration

  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();

    static float temp = 20.0f;
    temp += 0.5f;
    if (temp > 30.0f)
      temp = 20.0f;

    // publishSensor(name, value, retain) — retain defaults to true (kept on broker)
    cd.publishSensor("temperature", temp);
  }
}
