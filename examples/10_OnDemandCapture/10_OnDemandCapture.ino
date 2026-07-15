// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 10: On-Demand Capture (ESP32-CAM)
//
// Complements 07_SendImage (periodic upload): here the device sits idle until the
// dashboard/AI pushes a value of 1.0 (true) to the designated capture control variable.
// Then it captures one frame and uploads it via the sendImage() HTTP API.
//
// Targets ESP32 boards with an OV2640 camera (AI-Thinker ESP32-CAM pin map below —
// adjust the pins if your board differs). Open Serial Monitor at 115200 for [CD] debug
// output.

#if !defined(ESP32)
#error "This example needs an ESP32-CAM (esp_camera.h is ESP32-only)."
#endif

#include <WiFi.h>
#include <esp_camera.h>
#include <CircuitDigestCloud.h>

// ── Fill these in ───────────────────────────────────────────────────────────
const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";

const char* DEVICE_ID      = "your-physical-device-id";   // from the device setup panel
const char* CONNECTION_KEY = "your-connection-key";        // MQTT password
const char* API_KEY        = "cd_live_xxxxxxxxxxxxxxxx";   // dashboard API key (for uploads)
const char* CAPTURE_SLOT   = "capture-1";  // control variable slot (boolean catalog key)
// ────────────────────────────────────────────────────────────────────────────

// AI-Thinker ESP32-CAM pin map — change these if you're on a different board.
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

CircuitDigestCloud cd;

bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format  = PIXFORMAT_JPEG;
  config.frame_size    = FRAMESIZE_SVGA;   // 800x600 — comfortably under the 5MB upload cap
  config.jpeg_quality  = 12;               // 0-63, lower = higher quality/bigger file
  config.fb_count      = 1;

  return esp_camera_init(&config) == ESP_OK;
}

// Control callback for the "capture" trigger.
void handleCapture(float value) {
  // If the value is 0 (off), ignore it
  if (value < 0.5f) {
    return;
  }

  Serial.println("[CD] Capture triggered!");
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CD] capture failed: esp_camera_fb_get() returned null");
  } else {
    // sendImage manages TLS internally using Anedya/CircuitDigest credentials
    bool ok = cd.sendImage(fb->buf, fb->len, "image/jpeg", "capture.jpg");
    Serial.printf("[CD] sendImage: %s (err=%d)\n", ok ? "OK" : "FAILED", cd.lastError());
    esp_camera_fb_return(fb);

    // Reset the capture variable state to 0 (false) so the dashboard
    // button or trigger updates its state back to inactive.
    cd.publish(CAPTURE_SLOT, 0.0f);
  }
}

void setup() {
  Serial.begin(115200);

  if (!initCamera()) {
    Serial.println("Camera init failed — halting.");
    while (true) delay(1000);
  }

  cd.setDebug(&Serial);

  // Register the subscription to CAPTURE_SLOT
  cd.subscribe(CAPTURE_SLOT, handleCapture);

  // Connect to WiFi, set up MQTT, and start the background sync state machine.
  if (!cd.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, API_KEY)) {
    Serial.println("Initialization failed!");
  }
}

void loop() {
  cd.loop();   // drives the background library state machine
}
