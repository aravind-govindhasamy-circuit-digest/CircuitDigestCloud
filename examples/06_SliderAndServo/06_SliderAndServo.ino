// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
// CircuitDigestCloud — Example 06: Slider and Servo
//
// Reads a slider value (0–100) from the dashboard, maps it to a servo angle
// (0–180°), moves the servo, and publishes the actual angle back to the
// dashboard so a gauge widget can display the current position.
//
// Dashboard setup:
//   analog-input-1 (direction: output, type: float) — bind a Slider widget (0–100)
//   analog-input-2 (direction: input,  type: float) — bind a Gauge widget (0–180)
//
// Requires: ESP32Servo library for ESP32, or Servo.h for other boards.
// Works on: ESP32, ESP8266, Arduino UNO R4 WiFi, Raspberry Pi Pico W / Pico 2 W.

#include <CircuitDigestCloud.h>
#if defined(ESP32)
  #include <ESP32Servo.h>
#else
  #include <Servo.h>
#endif

// ── Fill in your credentials ────────────────────────────────────────────────
#define WIFI_SSID      "your_ssid"
#define WIFI_PASS      "your_password"
#define DEVICE_ID      "your-device-id"
#define CONNECTION_KEY "your-connection-key"
#define API_KEY        "cd_live_xxxxxxxxxxxx"   // API Key (dashboard)

#define MySlider  "analog-input-1"   // dashboard slider (0–100)  → device
#define MyAngle   "analog-input-2"   // servo angle     (0–180°)  → dashboard
// ────────────────────────────────────────────────────────────────────────────

// ── Servo config ─────────────────────────────────────────────────────────────
#define SERVO_PIN  13   // PWM-capable pin connected to servo signal wire
// ────────────────────────────────────────────────────────────────────────────

Servo servo;
CircuitDigestCloud CDcloud;

bool  pendingPublish = false;
float targetGaugeVal = 90.0f;        

// Kept as float to perfectly match: void (*)(float)
void onSlider(float value) {
  float temp = constrain(value, 0.0f, 100.0f);
  
  int physicalAngle = (int)(clamped * 1.8f); 
  servo.write(physicalAngle);

  targetGaugeVal = (float)physicalAngle; 
  pendingPublish = true;

  // Debug Printout to the local computer console
  Serial.print("Dashboard Slider: "); Serial.print(temp);Serial.println(" %");
  Serial.print("%  -> Real Servo Angle: "); Serial.print(physicalAngle);
  Serial.println(" deg");
}

void setup() {
  Serial.begin(115200);
  
  int timeout = 0;
  while (!Serial && timeout < 30) { 
    delay(100);
    timeout++;
  }
  Serial.println("\n--- Pico W Starting up ---");

  servo.attach(SERVO_PIN, 500, 2500); 
  servo.write(90); // Start at center physical position (90 degrees)

  // Link the dashboard slider to our tracking function
  CDcloud.subscribe(MySlider, onSlider);

  // Connect to Circuit Digest Cloud
  Serial.println("Connecting to Wi-Fi and Cloud Platform...");
  CDcloud.begin(WIFI_SSID, WIFI_PASS, DEVICE_ID, CONNECTION_KEY, API_KEY);
  
  CDcloud.publish(MyAngle, 90.0f);
  Serial.println("Initialization complete. Sent initial angle 90.0 to gauge.");
}

void loop() {
  // Keep the cloud communication alive
  CDcloud.loop();

  // If a new slider position updated targetGaugeVal, send it out now
  if (pendingPublish) {
    pendingPublish = false;
    
    bool success = CDcloud.publish(MyAngle, targetGaugeVal);
    
    if(success) {
      Serial.print("Successfully updated cloud gauge with Angle: ");
      Serial.print(targetGaugeVal);
      Serial.println(".0°");
    } else {
      Serial.println("Error: Failed to transmit angle payload to cloud.");
    }
  }
  
  delay(15);
}
