// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 10: On-Demand Capture (ESP32-CAM)
//
// Complements 07_SendImage (periodic upload): here the device sits idle until the
// dashboard/AI pushes a value of 1.0 (true) to the designated capture control variable.
// Then it captures one frame and uploads it via the sendImage() HTTP API.
//
// Targets ESP32 boards with an OV2640 camera. Open Serial Monitor at 115200 for [CD] debug
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

#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    47
#define CAM_PIN_SIOD    14
#define CAM_PIN_SIOC    13

#define CAM_PIN_D7      16   // Y9
#define CAM_PIN_D6      17   // Y8
#define CAM_PIN_D5      18   // Y7
#define CAM_PIN_D4      12   // Y6
#define CAM_PIN_D3      10   // Y5
#define CAM_PIN_D2      8    // Y4
#define CAM_PIN_D1      9    // Y3
#define CAM_PIN_D0      11   // Y2
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13

CircuitDigestCloud cd;

bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = CAM_PIN_D0;  config.pin_d1 = CAM_PIN_D1;
  config.pin_d2 = CAM_PIN_D2;  config.pin_d3 = CAM_PIN_D3;
  config.pin_d4 = CAM_PIN_D4;  config.pin_d5 = CAM_PIN_D5;
  config.pin_d6 = CAM_PIN_D6;  config.pin_d7 = CAM_PIN_D7;
  config.pin_xclk  = CAM_PIN_XCLK;
  config.pin_pclk  = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href  = CAM_PIN_HREF;
  config.pin_sscb_sda = CAM_PIN_SIOD;
  config.pin_sscb_scl = CAM_PIN_SIOC;
  config.pin_pwdn  = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;
  config.xclk_freq_hz = 20000000;
  config.pixel_format  = PIXFORMAT_JPEG;
  
  // High performance config using ESP32-S3 PSRAM
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_SVGA;   // 800x600 resolution
    config.jpeg_quality  = 12;               // High crisp detailing
    config.fb_count      = 2;                // Double buffering utilizing S3 PSRAM
    config.grab_mode     = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size   = FRAMESIZE_VGA;
    config.jpeg_quality  = 12;
    config.fb_count      = 1;
    config.grab_mode     = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) return false;

  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 1);     // Mirror orientation fixes
  s->set_hmirror(s, 1);

  return true;
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
  if (!psramInit()) {
    Serial.begin(115200);
    Serial.println("Warning: PSRAM Allocation failed!");
  }

  Serial.begin(115200);

  if (!initCamera()) {
    Serial.println("Camera init failed — halting.");
    while (true) delay(1000);
  }
  Serial.println("Camera hardware configured successfully.");

  cd.setDebug(&Serial);

  // Register the subscription to CAPTURE_SLOT (capture-1)
  cd.subscribe(CAPTURE_SLOT, handleCapture);

  // Connect to WiFi, set up MQTT, and start the background sync state machine.
  if (!cd.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, API_KEY)) {
    Serial.println("Initialization failed!");
  }
}

void loop() {
  cd.loop();   // drives the background library state machine
}
