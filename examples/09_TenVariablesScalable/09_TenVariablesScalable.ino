// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 09: Ten Variables, Scalable & Secure
//
// Demonstrates the recommended way to ship many variables:
//   * one table (SENSORS[]) describes every variable — add a row, not a code
//   block
//   * register + publish run in loops, so 10 variables is the same code as 100
//   * TLS validates the broker certificate in production (pinned CA), with a
//     single DEV_INSECURE switch for bench testing
//
// Each `slot` is a catalog key you create on the dashboard's device setup panel
// (hyphens only — Anedya keys can't contain underscores). Create all 10 slots
// below before running, or comment out the rows you haven't created yet.

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
const char *WIFI_SSID = "Semicon ACT";
const char *WIFI_PASS = "cracksen1605";
const char *DEVICE_ID =
    "3dadccf3-9fc3-4ad6-895f-b829bcf31a92"; // Physical Device ID (device setup
                                            // panel)
const char *CONNECTION_KEY =
    "c6bda40994173099a37c2fc9a5ed2ba9"; // Connection Key (device setup panel)

// 1 = skip certificate validation (bench/dev only).
// 0 = validate the broker against ANEDYA_ROOT_CA below (production).
#define DEV_INSECURE 1

// Paste the Anedya broker's root CA (PEM) here for production (DEV_INSECURE 0).
// Get it from the broker host's certificate chain; ESP8266 also needs NTP time
// (set automatically below) so the validity dates can be checked.
static const char ANEDYA_ROOT_CA[] PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----
... paste the Anedya root CA certificate here ...
-----END CERTIFICATE-----
)CERT";
// ---------------------------------------------------------------------------

// ---- Variable table — add a row to add a variable -------------------------
// name : friendly name used in your sketch
// slot : catalog key from the dashboard (direction: input / sensor)
// type : CD_FLOAT | CD_INT | CD_BOOL
struct VarDef {
  const char *name;
  const char *slot;
  CDType type;
};

const VarDef SENSORS[] = {
    {"temperature", "temperature-1", CD_FLOAT},
    {"humidity", "humidity-1", CD_FLOAT},
    {"pressure", "pressure-1", CD_FLOAT},
    {"voltage", "voltage-1", CD_FLOAT},
    {"current", "current-1", CD_FLOAT},
    {"power", "power-1", CD_FLOAT},
    {"lux", "analog-input-1", CD_INT}, // no "lux" family in catalog — generic analog slot
    {"co2", "analog-input-2", CD_INT}, // no "co2" family in catalog — generic analog slot
    {"count", "count-1", CD_INT},
    {"presence", "motion-1", CD_BOOL},
};
const size_t SENSOR_COUNT = sizeof(SENSORS) / sizeof(SENSORS[0]);
// ---------------------------------------------------------------------------

WiFiClientSecure net;
CircuitDigestCloud cd(net);

// Apply TLS config — called on startup AND after each transport reset, because
// WiFiClientSecure forgets setInsecure()/setCACert() on stop().
void applyTls() {
#if DEV_INSECURE
  net.setInsecure(); // dev only — skips certificate validation
#elif defined(ESP32)
  net.setCACert(ANEDYA_ROOT_CA);
#elif defined(ESP8266)
  static BearSSL::X509List ca(ANEDYA_ROOT_CA);
  net.setTrustAnchors(&ca);
  configTime(0, 0,
             "pool.ntp.org"); // ESP8266 needs real time to check cert dates
#endif
}

void resetTransport() {
  net.stop();
  applyTls();
}

// Replace these with real sensor reads. Returns a value per variable index so a
// single loop can feed every slot; type conversion happens at publish time.
float readSensor(size_t i) {
  (void)i;
  // Fully random each call — whole 0..999 plus a 2-decimal fraction.
  // No linear ramp, no repeating period.
  return random(0, 1000) + random(0, 100) / 100.0f;
}

void setup() {
  Serial.begin(115200);
  delay(200);

#if defined(ESP32)
  randomSeed(esp_random());              // hardware RNG
#else
  randomSeed(analogRead(A0) ^ micros()); // ESP8266: floating-pin noise + timer
#endif

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println(" connected");

#if defined(ESP32)
  WiFi.setSleep(false); // disable modem sleep — stops idle-gap TLS/MQTT drops
#else
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // ESP8266 equivalent
#endif

  applyTls();
  cd.setTransportResetCallback(resetTransport);

  cd.setCredentials(DEVICE_ID, CONNECTION_KEY);
  cd.setDebug(&Serial); // comment out for production

  // Register all variables from the table — scales with the array, not the
  // code.
  for (size_t i = 0; i < SENSOR_COUNT; i++) {
    cd.registerVariable(SENSORS[i].name, SENSORS[i].type, SENSORS[i].slot);
  }

  // Controls work the same way — see examples 01 / 05 for cd.onChange(...).
  cd.begin();
}

// Gap between two consecutive variable publishes. One variable goes out per
// gap, cycling the table — so the full set of SENSOR_COUNT variables refreshes
// Publish ALL variables as one tight batch, then stay idle until the next cycle.
// Anedya persists every variable with this "burst then idle" pattern; a
// sustained trickle (one var every few hundred ms) made it keep only a couple
// of streams and freeze the rest. Keep the interval >= 5s so the burst isn't
// happening continuously.
#define PUBLISH_INTERVAL_MS 10000 // full set every 10s

void loop() {
  cd.loop(); // drives connection + MQTT pump — call every iteration

  static uint32_t last = 0;
  if (millis() - last >= PUBLISH_INTERVAL_MS) {
    last = millis();

    // Publish every variable in one pass, picking the overload by declared type.
    for (size_t i = 0; i < SENSOR_COUNT; i++) {
      float r = readSensor(i);
      switch (SENSORS[i].type) {
      case CD_INT:
        cd.publishVariable(SENSORS[i].name, (int)r);
        break;
      case CD_BOOL:
        // Boolean slot stores numeric 1/0 — random on/off (not JSON true/false).
        cd.publishVariable(SENSORS[i].name, (int)((long)r & 1));
        break;
      case CD_FLOAT:
      default:
        cd.publishVariable(SENSORS[i].name, r);
        break;
      }
      cd.loop(); // pump between publishes so each flushes through TLS/MQTT
    }
  }
}
