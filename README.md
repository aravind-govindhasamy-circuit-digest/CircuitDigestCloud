# CircuitDigestCloud

Arduino library for connecting any Arduino-core device to the CircuitDigest Cloud MQTT platform. Transport-agnostic — works with WiFi, Ethernet, GSM, or any `Client`-compatible transport.

---

## Quick Start

```cpp
#include <WiFi.h>
#include <CircuitDigestCloud.h>

WiFiClient net;
CircuitDigestCloud cd(net);

void setup() {
    Serial.begin(115200);
    WiFi.begin("ssid", "password");
    while (WiFi.status() != WL_CONNECTED) delay(200);

    cd.setCredentials("uuid", "devid", "key");
    cd.setDebug(&Serial);   // comment out for production
    cd.registerVariable("temperature", CD_FLOAT);
    cd.begin();
}

void loop() {
    cd.loop();
    static uint32_t last = 0;
    if (millis() - last > 5000) {
        last = millis();
        cd.publishVariable("temperature", 25.3f);
    }
}
```

---

## Installation

**Arduino IDE Library Manager:** Search for `CircuitDigestCloud`.

**Manual ZIP:** Download, unzip into `~/Documents/Arduino/libraries/CircuitDigestCloud/`.

**PlatformIO:**
```ini
lib_deps = CircuitDigestCloud
```

**Required dependency:** [PubSubClient](https://github.com/knolleary/pubsubclient) ≥ 2.8 (installed automatically via Library Manager).

---

## Credentials

Find your credentials in the CircuitDigest Cloud dashboard:

| Parameter | Dashboard field |
|---|---|
| `uuid` | User UUID |
| `devid` | Device ID |
| `key` | Device Key |

---

## API Reference

| Method | Description |
|---|---|
| `CircuitDigestCloud(Client&)` | Constructor. Pass your transport client. |
| `setCredentials(uuid, devid, key)` | Set MQTT credentials. Call before `begin()`. Returns `false` if any field is empty. |
| `setBufferSize(bytes)` | PubSubClient buffer size. Default 512, min 256. |
| `setHeartbeatInterval(seconds)` | Heartbeat cadence. Default 60. `0` disables. |
| `setAutoAck(bool)` | Global auto-ack default. Default `true`. |
| `setDebug(Stream*)` | Enable debug logging (e.g. `&Serial`). `nullptr` disables. |
| `begin()` | Initialize. Returns `false` on bad credentials. Connection is lazy. |
| `loop()` | Call every iteration of `loop()`. Drives connection, MQTT pump, heartbeat. |
| `connected()` | Returns `true` when MQTT is up. |
| `lastError()` | Last `CDError` code. Read immediately after a method returns `false`. |
| `registerVariable(name, type)` | Pre-register a sensor variable (optional). |
| `onChange(name, cb, ack, type)` | Register a control callback. |
| `onChange(cb)` | Global fallback for unregistered controls. |
| `publishVariable(name, value, retain)` | Publish sensor reading. Accepts int/long/float/double/bool/const char*. `retain` (default `true`) keeps the message on the broker. |
| `ackChange(name, value)` | Publish control acknowledgement (use with `CD_ACK_MANUAL`). |

---

## Variable Types

| Constant | Wire format | C++ type |
|---|---|---|
| `CD_INT` | bare number | `long` |
| `CD_FLOAT` | bare number | `float` |
| `CD_BOOL` | `true`/`false` | `bool` |
| `CD_STRING` | quoted string | `const char*` |
| `CD_ENUM` | quoted string (semantic alias) | `const char*` |
| `CD_AUTO` | auto-detected on first use | — |

### `CDValue` — inbound payload

```cpp
void handleControl(const char* var, CDValue v) {
    // Type check
    v.type();       // CDType enum value
    v.isInt();      // true if CD_INT
    v.isFloat();    // true if CD_FLOAT
    v.isBool();     // true if CD_BOOL
    v.isString();   // true if CD_STRING or CD_ENUM

    // Value extraction (cross-type conversion applies — see below)
    v.asInt();      // long
    v.asFloat();    // float
    v.asBool();     // bool
    v.asString();   // const char* — VALID ONLY DURING THIS CALLBACK
}
```

Cross-type conversion rules:
- `asInt()` from float → truncated toward zero; from bool → 0 or 1
- `asFloat()` from int → exact promotion; from bool → 0.0 or 1.0
- `asBool()` from int/float → `value != 0`; from string → `true` only if text is `"true"`
- `asString()` from non-string types → returns `nullptr`

> **String lifetime warning:** `v.asString()` points into an internal buffer overwritten on the next inbound message. Copy it if you need it to persist:
> ```cpp
> char buf[32];
> strncpy(buf, v.asString(), sizeof(buf) - 1);
> ```

---

## Auto-Ack vs Manual Ack

- **`CD_ACK_AUTO`** (default): library publishes the ack automatically after your callback returns, echoing the received value.
- **`CD_ACK_MANUAL`**: library does not ack. You must call `cd.ackChange(name, actualValue)` — typically after reading back the hardware state.

```cpp
cd.onChange("relay", handleRelay, CD_ACK_MANUAL);

void handleRelay(const char* var, CDValue v) {
    digitalWrite(RELAY_PIN, v.asBool() ? HIGH : LOW);
    bool actual = digitalRead(RELAY_PIN) == HIGH;
    cd.ackChange("relay", actual);   // ack with real state
}
```

---

## Heartbeat / `state/last_seen`

The library publishes an empty message to `<base>/state/last_seen` every `heartbeatInterval` seconds. The server records the timestamp. Disable with `cd.setHeartbeatInterval(0)`.

---

## Reconnect & Backoff

On disconnect, the library reconnects with exponential backoff: 1 s, 2 s, 4 s, 8 s, 16 s, 30 s, 30 s, … Reset to 1 s on success. `loop()` never blocks — no `delay()` calls inside.

---

## Debug Logging

Off by default. Enable with:
```cpp
cd.setDebug(&Serial);
```
Every log line is prefixed `[CD] `. Disable for production.

---

## One Instance Per Sketch

Only one `CircuitDigestCloud` instance is supported per sketch (v1.0.4). A second instance overwrites the internal MQTT callback pointer, breaking the first. The library uses your `MQTT_DEVICE_ID` as the MQTT client ID, so each device connects with a unique identity.

---

## Examples

Five examples are included under `examples/`:

| Sketch | What it shows |
|---|---|
| `01_SensorAndControl` | Publish a temperature sensor + control GPIO 2 from the dashboard — **start here** |
| `02_BasicSensor` | Publish a single float sensor every 5 seconds, no controls |
| `03_BasicControl` | Receive a boolean control and drive `LED_BUILTIN`, auto-ack |
| `04_AllVariableTypes` | All five types as sensors and controls in one sketch |
| `05_ManualAckAndManyControls` | Manual ack with GPIO read-back, mixed ack modes, global fallback |

All examples target **ESP32 and ESP8266**. Fill in your WiFi credentials and dashboard keys at the top of each sketch, then open Serial Monitor at 115200 baud to see `[CD]` debug output.

---

## Acknowledgements

This library builds on [PubSubClient](https://github.com/knolleary/pubsubclient) by **Nick O'Leary**, which handles the underlying MQTT 3.1.1 transport. PubSubClient is licensed under the MIT License.

---
## License

MIT — Copyright (c) 2026 Jobit Joseph, Circuit Digest. See [LICENSE](LICENSE).
