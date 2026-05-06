# CircuitDigestCloud — Functional Specification Document (FSD)

**Library name:** `CircuitDigestCloud`
**Version target:** 1.0.0
**Document version:** 1.0
**Status:** Authoritative — this document is sufficient to (re)build the library from scratch with no additional information.

---

## 1. Document Purpose & Scope

This FSD specifies an Arduino-compatible C++ library that connects an embedded device to the CircuitDigest Cloud MQTT broker (`mqtt.circuitdigest.cloud:1883`) and provides a high-level API for publishing sensor readings, receiving control commands from the dashboard, acknowledging those commands, and maintaining device-presence state (`state/online`, `state/last_seen`).

The library is **transport-agnostic**: it does not initiate WiFi/GSM/Ethernet/4G connectivity itself. The application code is responsible for bringing up network connectivity and providing a connected `Client&` reference (e.g. `WiFiClient`, `WiFiClientSecure`, `EthernetClient`, `TinyGsmClient`). This keeps the library compatible with any current or future Arduino-core target.

Out of scope:
- Bringing up WiFi/GSM/Ethernet (caller's responsibility).
- TLS configuration (caller's responsibility on the `Client` they pass in; the broker is plain TCP at 1883 in this spec).
- OTA updates, file system, or any non-MQTT cloud feature.

---

## 2. Library Identity & File Layout

### 2.1 Names

| Element | Value |
|---|---|
| Folder name | `CircuitDigestCloud` |
| Header | `CircuitDigestCloud.h` |
| Implementation | `CircuitDigestCloud.cpp` |
| Public class | `CircuitDigestCloud` |
| Suggested instance name in user code | `cd` |

### 2.2 Required directory layout

```
CircuitDigestCloud/
├── library.properties
├── library.json                 (optional, for PlatformIO discovery)
├── keywords.txt                 (Arduino IDE syntax highlighting)
├── LICENSE
├── README.md
├── src/                         (MANDATORY — Arduino IDE 1.5+ layout)
│   ├── CircuitDigestCloud.h
│   ├── CircuitDigestCloud.cpp
│   ├── CDValue.h
│   ├── CDValue.cpp
│   ├── CDTypes.h                (enums: CDType, CDAckMode, CDDirection, CDError)
│   ├── CDJson.h                 (internal minimal JSON helpers)
│   └── CDJson.cpp
└── examples/
    ├── 01_BasicSensor/01_BasicSensor.ino
    ├── 02_BasicControl/02_BasicControl.ino
    ├── 03_AllVariableTypes/03_AllVariableTypes.ino
    └── 04_ManualAckAndManyControls/04_ManualAckAndManyControls.ino
```

The `src/` folder is mandatory. The library uses the modern Arduino IDE 1.5+ recursive layout so PlatformIO with the Arduino framework also discovers it correctly.

### 2.3 Renaming the library

To rename (e.g. from `CircuitDigestCloud` to `CDCloud`), exactly four touch points must be changed:

1. The folder name (`CircuitDigestCloud/` → `CDCloud/`).
2. The header/source filenames (`CircuitDigestCloud.h/.cpp` → `CDCloud.h/.cpp`).
3. The class name in those files.
4. The `name=` field in `library.properties`.

No other internal symbols depend on the library name. All internal types are prefixed `CD` and never include the full library name.

### 2.4 `library.properties` (Arduino library manager metadata)

```
name=CircuitDigestCloud
version=1.0.0
author=CircuitDigest
maintainer=CircuitDigest <support@circuitdigest.com>
sentence=Arduino client for the CircuitDigest Cloud MQTT platform.
paragraph=Transport-agnostic high-level wrapper around PubSubClient providing variable registration, typed publish, command callbacks with auto-acknowledgement, last-will online status, heartbeat, and reconnection. Works with any Arduino-core board (ESP32, ESP8266, RP2040, AVR, SAMD, nRF52, etc.) over any Client transport.
category=Communication
url=https://github.com/circuitdigest/CircuitDigestCloud
architectures=*
depends=PubSubClient
includes=CircuitDigestCloud.h
```

### 2.5 `keywords.txt`

Tab-separated (Arduino IDE convention; spaces will not work):

```
CircuitDigestCloud	KEYWORD1
CDValue	KEYWORD1
CDType	KEYWORD1
CDAckMode	KEYWORD1
CDError	KEYWORD1

setCredentials	KEYWORD2
setBufferSize	KEYWORD2
setHeartbeatInterval	KEYWORD2
setAutoAck	KEYWORD2
setDebug	KEYWORD2
begin	KEYWORD2
loop	KEYWORD2
connected	KEYWORD2
registerSensor	KEYWORD2
onControl	KEYWORD2
publishSensor	KEYWORD2
ackControl	KEYWORD2
lastError	KEYWORD2

CD_AUTO	LITERAL1
CD_INT	LITERAL1
CD_FLOAT	LITERAL1
CD_BOOL	LITERAL1
CD_STRING	LITERAL1
CD_ENUM	LITERAL1
CD_ACK_AUTO	LITERAL1
CD_ACK_MANUAL	LITERAL1
```

---

## 3. Dependencies & Build Compatibility

### 3.1 Required dependency

- **PubSubClient** (Nick O'Leary), version 2.8 or later.
  Declared via `depends=PubSubClient` in `library.properties` and via `lib_deps` in `library.json`.

No other libraries are required. The library does **not** depend on ArduinoJson; a minimal internal JSON helper handles the constrained payload shapes only.

### 3.2 Target platforms

`architectures=*` — must compile on any Arduino-core target that PubSubClient itself supports. The library uses only:
- Standard C++ allowed by the Arduino core (no C++17-only features; aim for C++11 compatibility — SAMD and AVR cores typically use C++11).
- `String` (Arduino built-in) — used sparingly; topic strings are built into a single fixed buffer where possible to avoid heap fragmentation.
- `<stdint.h>`, `<stddef.h>`, `<string.h>`, `<stdio.h>`, `<stdlib.h>` from the Arduino core.
- `Client` and `Stream` from the Arduino core.

The library must **not** include WiFi-specific headers in its own source; the user includes those in their sketch.

### 3.3 Reference targets for testing

ESP8266 (NodeMCU, Wemos D1 Mini) and ESP32 family (ESP32, ESP32-S2, ESP32-S3, ESP32-C3) over WiFi using `WiFiClient`. All examples target both via `#ifdef ESP32` / `#ifdef ESP8266`.

---

## 4. Architectural Overview

### 4.1 Component diagram (textual)

```
┌──────────────────── User sketch ────────────────────┐
│                                                     │
│   WiFi.begin(...)         (transport bring-up)      │
│   WiFiClient net;                                   │
│   CircuitDigestCloud cd(net);                       │
│   cd.setCredentials(device, uuid, devid, key);      │
│   cd.onControl("light_1", handleLight);             │
│   cd.begin();                                       │
│   loop() { cd.loop(); cd.publishSensor(...); }      │
│                                                     │
└──────────────────────────┬──────────────────────────┘
                           │
                           ▼
┌─────────────────── CircuitDigestCloud ──────────────┐
│  - Topic builder     (cached base path)             │
│  - Variable registry (linked list)                  │
│  - Connection state machine + backoff               │
│  - Last-will + online publish                       │
│  - Heartbeat scheduler                              │
│  - Inbound dispatcher (PubSubClient callback)       │
│  - JSON formatter / parser (minimal)                │
│  - Auto-ack engine                                  │
└──────────────────────────┬──────────────────────────┘
                           │
                           ▼
                    ┌──────────────┐
                    │ PubSubClient │   (MQTT 3.1.1 over Client)
                    └──────┬───────┘
                           ▼
                       Client&     (WiFiClient / TinyGsmClient / ...)
                           ▼
                     mqtt.circuitdigest.cloud:1883
```

### 4.2 Threading / re-entrancy

The library is single-threaded and intended to be driven by `loop()`. It is **not** safe to call its methods from an ISR or from a different FreeRTOS task than the one that calls `cd.loop()`, unless the user provides external mutual exclusion. This must be stated in the README.

### 4.3 Memory model

- One `CircuitDigestCloud` instance owns one `PubSubClient`.
- Variable registry: singly linked list of `CDVariable` nodes, allocated with `new` at registration time. Never freed (registration is a startup op). Document this.
- Variable name pointers: stored as `const char*`. The caller must guarantee the lifetime of each name (string literals satisfy this trivially; this is the standard Arduino-library convention).
- Topic-build scratch buffer: one fixed `char` buffer of `CD_TOPIC_BUFFER_SIZE` (default 160) bytes, reused for every publish/subscribe. Sized to fit `cd/users/<36>/devices/<36>/control/<32>/get` plus headroom.
- Payload-build scratch buffer: one fixed `char` buffer of `CD_PAYLOAD_BUFFER_SIZE` (default 96) bytes, reused for every publish.
- Inbound payload: PubSubClient delivers a `byte*` + length; the library reads it directly without copying for parsing. The string slot of `CDValue`, when type is `CD_STRING`, points into a separate per-message scratch buffer of `CD_INBOUND_STRING_BUFFER` (default 64) bytes that is null-terminated and valid only during the callback.

---

## 5. Hardcoded Server Configuration

These values are compiled into the library and **not** user-configurable through the public API:

```cpp
// Inside CircuitDigestCloud.cpp
static const char  CD_MQTT_HOST[] = "mqtt.circuitdigest.cloud";
static const uint16_t CD_MQTT_PORT = 1883;
static const char  CD_USERNAME_PREFIX[] = "mqtt_u_";
static const char  CD_TOPIC_ROOT[] = "cd/users/";
static const char  CD_TOPIC_DEVICES[] = "/devices/";
```

A user wishing to change the host must edit the source. This is intentional per the requirement that "the user not need to change it."

---

## 6. Type System

### 6.1 `CDType` enum (in `CDTypes.h`)

```cpp
enum CDType : uint8_t {
    CD_AUTO   = 0,   // Type unknown; will be detected and locked on first use
    CD_INT    = 1,   // Whole number, stored as long
    CD_FLOAT  = 2,   // Floating-point, stored as float
    CD_BOOL   = 3,   // true / false
    CD_STRING = 4,   // Quoted JSON string
    CD_ENUM   = 5    // Treated as CD_STRING on the wire; semantic hint only
};
```

`CD_ENUM` is wire-identical to `CD_STRING` (JSON quoted token). It exists as a hint so user code can document intent and so debug logs can show the right label.

### 6.2 `CDAckMode` enum

```cpp
enum CDAckMode : uint8_t {
    CD_ACK_AUTO   = 0,   // Library publishes ack immediately after callback returns
    CD_ACK_MANUAL = 1    // User must call ackControl(...) inside or after the callback
};
```

### 6.3 `CDDirection` enum (internal)

```cpp
enum CDDirection : uint8_t {
    CD_DIR_SENSOR  = 0,  // Outbound: device → dashboard
    CD_DIR_CONTROL = 1   // Inbound:  dashboard → device, with ack
};
```

A variable's direction is decided at registration (sensor vs control) and never changes. Bidirectional variables are supported by registering the same name as both a sensor and a control.

### 6.4 `CDError` enum

Returned by `lastError()`; the most recent failure reason.

```cpp
enum CDError : uint8_t {
    CD_OK                       = 0,
    CD_ERR_NOT_INITIALIZED      = 1,  // begin() not called or failed
    CD_ERR_BAD_CREDENTIALS      = 2,  // any of the four credential fields null/empty
    CD_ERR_NOT_CONNECTED        = 3,  // tried to publish while disconnected
    CD_ERR_PUBLISH_FAILED       = 4,  // PubSubClient publish() returned false
    CD_ERR_TOPIC_TOO_LONG       = 5,  // topic build exceeded buffer
    CD_ERR_PAYLOAD_TOO_LONG     = 6,  // payload build exceeded buffer
    CD_ERR_INVALID_JSON         = 7,  // inbound JSON could not be parsed
    CD_ERR_TYPE_MISMATCH        = 8,  // inbound value did not match locked type
    CD_ERR_UNKNOWN_VARIABLE     = 9,  // ack/manual op for unregistered variable
    CD_ERR_OUT_OF_MEMORY        = 10  // registry node allocation failed
};
```

---

## 7. `CDValue` — typed payload wrapper

Defined in `CDValue.h`. Used only for inbound control commands and (internally) for ack values when the user supplies a manual ack.

### 7.1 Public interface

```cpp
class CDValue {
public:
    CDValue();                     // type = CD_AUTO, no value

    CDType type() const;

    bool isInt()    const;
    bool isFloat()  const;
    bool isBool()   const;
    bool isString() const;

    long          asInt()    const;   // 0 if not numeric
    float         asFloat()  const;   // 0.0f if not numeric
    bool          asBool()   const;   // false if not numeric/bool; "true"→true, "false"→false for strings
    const char*   asString() const;   // valid only during the callback in which it was delivered

private:
    CDType _type;
    union {
        long  i;
        float f;
        bool  b;
    } _v;
    const char* _s;   // points into CDInboundStringBuffer; null when not CD_STRING
};
```

### 7.2 Conversion semantics

- `asInt()` from `CD_FLOAT` → truncated toward zero.
- `asFloat()` from `CD_INT` → exact promotion.
- `asBool()` from `CD_INT` or `CD_FLOAT` → `value != 0`.
- `asBool()` from `CD_STRING` → `true` if string is `"true"`, `false` if `"false"`, otherwise `false`.
- `asString()` from non-string types → returns `nullptr`.

### 7.3 String-pointer lifetime (CRITICAL)

When `CDValue::_type == CD_STRING`, `_s` points into a static internal buffer that is overwritten on the next inbound message. Users who need to keep the string must copy it (e.g. into their own `String` or `char[]`). This must be documented in:
- The header comment above `CDValue`.
- The README.
- Examples 03 and 04 (where strings are received).

---

## 8. Public API of `CircuitDigestCloud`

### 8.1 Type aliases

```cpp
typedef void (*CDControlCallback)(const char* variable, CDValue value);
```

### 8.2 Class declaration (canonical signature set)

```cpp
class CircuitDigestCloud {
public:
    // ---- Construction --------------------------------------------------
    explicit CircuitDigestCloud(Client& transport);

    // ---- Pre-begin configuration --------------------------------------
    bool setCredentials(const char* device,
                        const char* uuid,
                        const char* devid,
                        const char* key);

    void setBufferSize(uint16_t bytes);          // default 512
    void setHeartbeatInterval(uint32_t seconds); // default 60, 0 = disabled
    void setAutoAck(bool enabled);               // default true (global)
    void setDebug(Stream* stream);               // default nullptr (off)

    // ---- Initialization -----------------------------------------------
    bool begin();      // returns false on bad credentials; true otherwise

    // ---- Main loop ----------------------------------------------------
    void loop();       // call as often as possible from the sketch loop()

    // ---- Connection state ---------------------------------------------
    bool connected();
    CDError lastError();

    // ---- Variable registration (optional but recommended) -------------
    bool registerSensor(const char* name, CDType type = CD_AUTO);
    bool onControl(const char* name,
                   CDControlCallback cb,
                   CDAckMode ack = CD_ACK_AUTO,
                   CDType type = CD_AUTO);
    void onControl(CDControlCallback cb);   // global fallback

    // ---- Publishing sensors -------------------------------------------
    bool publishSensor(const char* name, int          value);
    bool publishSensor(const char* name, long         value);
    bool publishSensor(const char* name, float        value);
    bool publishSensor(const char* name, double       value);
    bool publishSensor(const char* name, bool         value);
    bool publishSensor(const char* name, const char*  value);

    // ---- Acknowledging control commands -------------------------------
    bool ackControl(const char* name, int          value);
    bool ackControl(const char* name, long         value);
    bool ackControl(const char* name, float        value);
    bool ackControl(const char* name, double       value);
    bool ackControl(const char* name, bool         value);
    bool ackControl(const char* name, const char*  value);

private:
    // see Section 11
};
```

### 8.3 Method-by-method behavior

#### `CircuitDigestCloud(Client& transport)`
Stores a reference to the transport. Constructs the internal `PubSubClient` over it. No network activity occurs.

#### `setCredentials(device, uuid, devid, key)`
Stores the four credential pointers (as `const char*`, no copy; caller must keep strings alive — string literals are fine).
Validation:
- All four pointers must be non-null and length > 0.
- `device` (MQTT client ID) is **mandatory** — empty/null returns `false` and sets `lastError() = CD_ERR_BAD_CREDENTIALS`. The library does **not** auto-generate a client ID.
- Returns `true` on success, `false` on validation failure.
Must be called before `begin()`. Calling after `begin()` is allowed but does not take effect until the next reconnect.

#### `setBufferSize(bytes)`
Sets the PubSubClient internal buffer size. Default applied at `begin()` is **512 bytes**. Minimum enforced floor: 256. Calls `pubsub.setBufferSize(bytes)`.

#### `setHeartbeatInterval(seconds)`
Sets the cadence at which `state/last_seen` is published. Default 60 seconds. Setting to 0 disables heartbeat entirely. Takes effect immediately.

#### `setAutoAck(enabled)`
Global default for control acknowledgement. Default `true`. Per-variable `CDAckMode` passed to `onControl(...)` overrides the global default for that variable only.

#### `setDebug(stream)`
Pass any `Stream*` (typically `&Serial`) to enable verbose logging. Pass `nullptr` to disable. Off by default. **Examples must explicitly mention this is opt-in and call `setDebug(&Serial)` so users see the logs during development.**

#### `begin()`
Returns `false` if credentials are missing/invalid (`lastError() = CD_ERR_BAD_CREDENTIALS`).
On success:
1. Builds and caches the topic base: `cd/users/<uuid>/devices/<devid>` into a strdup'd string for the lifetime of the instance.
2. Builds and caches the username: `mqtt_u_<devid>` into a strdup'd string.
3. Calls `pubsub.setServer(CD_MQTT_HOST, CD_MQTT_PORT)`.
4. Calls `pubsub.setBufferSize(_bufferSize)` (default 512).
5. Calls `pubsub.setCallback(<internal dispatcher>)`.
6. Sets internal state machine to `CD_STATE_DISCONNECTED`. Connection happens lazily on the first `loop()` call, not in `begin()`. This avoids blocking in `setup()` when network is not yet up.
7. Returns `true`.

`begin()` can be called more than once; it will reset state and re-cache.

#### `loop()`
Must be called frequently from the sketch's `loop()`. Drives:
1. The connection state machine (Section 12).
2. `pubsub.loop()` — pumps MQTT keepalives and inbound traffic.
3. The heartbeat scheduler — if `now - lastHeartbeat >= interval` and connected, publishes to `state/last_seen`.

`loop()` is non-blocking; it never `delay()`s.

#### `connected()`
Thin wrapper over `pubsub.connected()`. Returns `true` only when MQTT is currently up.

#### `lastError()`
Returns the most recent `CDError`. Cleared (set to `CD_OK`) at the start of every public call that sets it. Read it immediately after a method that returns `false`.

#### `registerSensor(name, type)`
Adds the variable to the registry as a sensor (outbound). Type defaults to `CD_AUTO`; if explicit, locks the type. Returns `false` only on out-of-memory or duplicate registration with a conflicting direction.

Calling `publishSensor("x", v)` without prior `registerSensor("x")` is allowed — the library auto-registers on first publish.

#### `onControl(name, cb, ack, type)` (per-variable)
Adds the variable to the registry as a control (inbound) and stores the callback + ack mode + (optionally) explicit type. Returns `true` on success, `false` on out-of-memory or null pointers. If the variable is already registered as a control, the callback/ack/type are **replaced**.

#### `onControl(cb)` (global fallback)
Registers a single global callback fired for any inbound `control/<var>/set` whose `<var>` is not in the registry. There can be only one global fallback; calling again replaces it. Pass `nullptr` to clear.

#### `publishSensor(name, value)` (six overloads)
1. If `connected() == false` → set `lastError() = CD_ERR_NOT_CONNECTED`, return `false`.
2. Look up (or auto-register) the variable as a sensor.
3. Type handling:
   - If the variable's type is `CD_AUTO`, the called overload determines the type and locks it.
   - If the variable's type is locked and the overload mismatches the locked type, the call still succeeds but if debug is on, log a warning. (This is permissive on outbound — the dashboard sees what the device sends.)
4. Build topic: `<base>/sensor/<name>`.
5. Build JSON payload: `{"<name>":<value>}` (formatting rules in Section 9).
6. `pubsub.publish(topic, payload, false)` (QoS 0, retain false). Return its result.

Numeric formatting:
- `int`/`long` → `%ld`.
- `float`/`double` → `%g` with 6 significant digits, trimmed trailing zeros after the decimal. `nan`/`inf` are emitted as JSON `null` (and `lastError()` set to `CD_ERR_PAYLOAD_TOO_LONG`? No — set a separate logical-warning flag; for v1.0 just emit `null`).
- `bool` → `true`/`false` (lowercase).
- `const char*` → JSON-escaped quoted string. Escapes: `\"`, `\\`, `\n`, `\r`, `\t`. Other control chars become `\u00XX`. UTF-8 passes through unchanged.

#### `ackControl(name, value)` (six overloads)
Same flow as `publishSensor` but:
- Topic: `<base>/control/<name>/get`.
- `pubsub.publish(topic, payload, true)` (QoS 1, retain true).
- If the variable is not a registered control and there is no global fallback registered, return `false` and set `lastError() = CD_ERR_UNKNOWN_VARIABLE`.

---

## 9. JSON Format (wire protocol)

### 9.1 Outbound (publish)

All sensor and ack publishes use the form:

```json
{"<variable_name>":<value>}
```

Exactly one key. The key is the variable name. Whitespace is omitted to keep payload small. Strings are quoted; numbers and booleans are bare.

### 9.2 LWT and online-status payloads

- LWT (registered with broker via `pubsub.connect`): topic `<base>/state/online`, payload `{"online":0}`, QoS 1, retain true.
- On every successful connect: publish to same topic, payload `{"online":1}`, QoS 1, retain true.

### 9.3 Heartbeat payload

- Topic: `<base>/state/last_seen`.
- Payload: empty string (`""`, length 0). The server stamps the timestamp from the receipt of the message.
- QoS 0, retain false.
- Cadence: every `heartbeatInterval` seconds while connected. The first heartbeat fires immediately after the first successful connect.

### 9.4 Inbound (control commands)

Dashboard always sends:

```json
{"<variable_name>":<value>}
```

The `<variable_name>` in the JSON key **must match** the `<var>` segment of the topic `control/<var>/set`. If they differ, the library prefers the topic segment (it matches the variable name registered) but the value is parsed from whatever single key is present in the JSON (since dashboard contract is one key per message). If multi-key payloads ever arrive, only the first key is used and a debug warning is logged.

### 9.5 Internal JSON parser specification (`CDJson.cpp`)

The parser is a hand-rolled state machine, NOT a general JSON parser. It accepts only:

```
value = '{' ws '"' key '"' ws ':' ws (number | string | 'true' | 'false') ws '}' ws
ws    = (' ' | '\t' | '\n' | '\r')*
```

Where:
- `key` may contain anything except `"` and `\`.
- `string` is `"` followed by zero or more characters (with backslash-escape support for `\"`, `\\`, `\n`, `\r`, `\t`, `\u00XX`) followed by `"`.
- `number` matches `[-]?[0-9]+([.][0-9]+)?([eE][-+]?[0-9]+)?` and is classified as `CD_INT` if no `.`/`e`/`E` is present, else `CD_FLOAT`.
- `null` is accepted and yields `CD_AUTO` with no value (callback not fired; debug warning).

Output: a `CDValue` plus the extracted key (written into a small fixed scratch buffer for comparison with the registry).

If parsing fails, the parser returns false; the dispatcher logs the failure and drops the message.

### 9.6 Internal JSON formatter (`CDJson.cpp`)

Provides:
```cpp
size_t cdFormatInt   (char* out, size_t cap, const char* key, long  value);
size_t cdFormatFloat (char* out, size_t cap, const char* key, float value);
size_t cdFormatBool  (char* out, size_t cap, const char* key, bool  value);
size_t cdFormatString(char* out, size_t cap, const char* key, const char* value);
```

Each writes the full `{"key":value}` form into `out` (null-terminated) and returns bytes written, or 0 on overflow. Keys are not escaped (variable names are constrained ASCII per dashboard rules).

---

## 10. MQTT Topic Conventions

All topics derive from a single cached base built once at `begin()`:

```
<base> = "cd/users/" + <uuid> + "/devices/" + <devid>
```

| Purpose | Topic | QoS | Retain | Direction |
|---|---|---:|---:|---|
| Online status (LWT + on-connect) | `<base>/state/online` | 1 | true | publish |
| Last-seen heartbeat | `<base>/state/last_seen` | 0 | false | publish |
| Sensor data | `<base>/sensor/<var>` | 0 | false | publish |
| Control set (subscribe wildcard) | `<base>/control/+/set` | 1 | — | subscribe |
| Control acknowledgement | `<base>/control/<var>/get` | 1 | true | publish |

The library subscribes to `<base>/control/+/set` exactly once after each successful connect. It does **not** subscribe to per-variable topics; the wildcard handles all controls.

`<var>` in the topic is the literal variable name (case-sensitive, no escaping).

---

## 11. Internal Data Structures

### 11.1 Variable registry node

```cpp
struct CDVariable {
    const char*       name;        // pointer-stored; caller-owned
    CDDirection       direction;   // CD_DIR_SENSOR or CD_DIR_CONTROL
    CDType            type;        // may be CD_AUTO until detected
    bool              typeLocked;  // true once set explicitly or detected
    CDControlCallback callback;    // controls only; nullptr otherwise
    CDAckMode         ackMode;     // controls only
    CDVariable*       next;
};
```

Singly linked list, head pointer in the class. Lookup is O(N) by `strcmp(name, ...)`. N is expected to be small (<32 typical).

A single name may exist twice in the list — once as `CD_DIR_SENSOR` and once as `CD_DIR_CONTROL` — to support bidirectional variables.

### 11.2 Class private members

```cpp
private:
    Client&        _transport;
    PubSubClient   _pubsub;

    const char*    _credDevice;    // MQTT client ID
    const char*    _credUuid;
    const char*    _credDevid;
    const char*    _credKey;

    char*          _topicBase;     // "cd/users/<uuid>/devices/<devid>"
    char*          _username;      // "mqtt_u_<devid>"
    uint16_t       _topicBaseLen;

    uint16_t       _bufferSize;
    uint32_t       _heartbeatInterval;   // seconds; 0 disables
    uint32_t       _lastHeartbeatMs;
    bool           _autoAck;

    Stream*        _debug;
    CDError        _lastError;

    enum State : uint8_t {
        CD_STATE_DISCONNECTED,
        CD_STATE_BACKOFF,
        CD_STATE_CONNECTING,
        CD_STATE_CONNECTED
    } _state;

    uint32_t       _backoffNextMs;
    uint16_t       _backoffSeconds;       // 1, 2, 4, 8, 16, 30, 30, ...

    CDVariable*           _registryHead;
    CDControlCallback     _globalControlCb;

    char           _topicScratch  [CD_TOPIC_BUFFER_SIZE];     // 160
    char           _payloadScratch[CD_PAYLOAD_BUFFER_SIZE];   //  96
    char           _inStringBuf   [CD_INBOUND_STRING_BUFFER]; //  64

    static CircuitDigestCloud* _instance;   // for static PubSubClient cb trampoline

    static void _staticCallback(char* topic, uint8_t* payload, unsigned int len);
    void        _onMqttMessage(char* topic, uint8_t* payload, unsigned int len);

    bool        _attemptConnect();
    void        _onConnected();
    void        _publishOnline(bool online);
    void        _publishHeartbeat();
    void        _scheduleBackoff();

    CDVariable* _findVariable(const char* name, CDDirection dir);
    CDVariable* _registerVariable(const char* name, CDDirection dir, CDType type);

    bool        _publishWithRetain(const char* fullTopic, const char* payload, bool retain);
    size_t      _buildTopicSensor (const char* var, char* out, size_t cap);
    size_t      _buildTopicCtlGet (const char* var, char* out, size_t cap);
    size_t      _buildTopicState  (const char* leaf, char* out, size_t cap);  // "online"|"last_seen"

    void        _logf(const char* fmt, ...);   // no-op if _debug == nullptr
```

### 11.3 Compile-time tunables (in `CDTypes.h`)

```cpp
#ifndef CD_TOPIC_BUFFER_SIZE
#define CD_TOPIC_BUFFER_SIZE 160
#endif

#ifndef CD_PAYLOAD_BUFFER_SIZE
#define CD_PAYLOAD_BUFFER_SIZE 96
#endif

#ifndef CD_INBOUND_STRING_BUFFER
#define CD_INBOUND_STRING_BUFFER 64
#endif

#ifndef CD_DEFAULT_BUFFER_SIZE
#define CD_DEFAULT_BUFFER_SIZE 512   // PubSubClient buffer
#endif

#ifndef CD_DEFAULT_HEARTBEAT_S
#define CD_DEFAULT_HEARTBEAT_S 60
#endif

#ifndef CD_BACKOFF_MAX_S
#define CD_BACKOFF_MAX_S 30
#endif
```

The user can override any of these with a `-D` flag in PlatformIO or a `build_flags` entry, or by `#define`-ing before `#include <CircuitDigestCloud.h>` (the header guards them appropriately).

---

## 12. Connection State Machine

States: `DISCONNECTED → BACKOFF → CONNECTING → CONNECTED → DISCONNECTED → ...`

State transitions, all driven by `loop()`:

| From | Event | To | Action |
|---|---|---|---|
| (any) | `begin()` called | `DISCONNECTED` | reset all backoff state |
| `DISCONNECTED` | `loop()` runs | `CONNECTING` | call `_attemptConnect()` |
| `CONNECTING` | connect succeeded | `CONNECTED` | call `_onConnected()`, reset `_backoffSeconds = 1` |
| `CONNECTING` | connect failed | `BACKOFF` | call `_scheduleBackoff()` |
| `BACKOFF` | `now >= _backoffNextMs` | `CONNECTING` | call `_attemptConnect()` |
| `CONNECTED` | `pubsub.connected() == false` | `DISCONNECTED` | log; on next loop iteration go to `CONNECTING` |
| `CONNECTED` | `loop()` runs | `CONNECTED` | call `_pubsub.loop()`, run heartbeat scheduler |

### 12.1 `_attemptConnect()`

1. If transport reports not connected (`_pubsub.state() == MQTT_CONNECT_FAILED` or similar), this still tries — PubSubClient itself manages the underlying TCP via the `Client&`. For transports that need explicit `Client::connect(host, port)`, PubSubClient handles it inside `pubsub.connect(...)`.
2. Build LWT topic into scratch: `<base>/state/online`.
3. LWT payload: literal `"{\"online\":0}"`.
4. Call `_pubsub.connect(_credDevice, _username, _credKey, lwtTopic, /*willQos*/1, /*willRetain*/true, /*willMessage*/"{\"online\":0}", /*cleanSession*/true)`.
5. If true → state = `CONNECTED`, call `_onConnected()`. If false → return false.

### 12.2 `_onConnected()`

In order:
1. Subscribe to `<base>/control/+/set` at QoS 1.
2. Publish `{"online":1}` to `<base>/state/online`, QoS 1, retain true.
3. Reset `_lastHeartbeatMs = 0` so a heartbeat fires on the next `loop()`.
4. Set `_backoffSeconds = 1` (success resets backoff).

### 12.3 `_scheduleBackoff()`

1. `_backoffNextMs = millis() + _backoffSeconds * 1000UL`.
2. Double `_backoffSeconds`, clamped to `CD_BACKOFF_MAX_S` (30 by default).
3. Sequence: 1, 2, 4, 8, 16, 30, 30, 30, …
4. The backoff is **per failure**, reset to 1 on success.

### 12.4 Heartbeat scheduler

Inside `loop()`, after `_pubsub.loop()`:

```cpp
if (_state == CD_STATE_CONNECTED && _heartbeatInterval > 0) {
    uint32_t nowMs = millis();
    if (_lastHeartbeatMs == 0 ||
        (uint32_t)(nowMs - _lastHeartbeatMs) >= _heartbeatInterval * 1000UL) {
        _publishHeartbeat();
        _lastHeartbeatMs = nowMs;
    }
}
```

Note unsigned subtraction handles the `millis()` rollover at ~49 days correctly.

### 12.5 `_publishHeartbeat()`

1. Build topic `<base>/state/last_seen`.
2. Call `_pubsub.publish(topic, (const uint8_t*)nullptr, (unsigned int)0, false)` — empty payload, QoS 0, retain false.
3. Log via debug if enabled.

---

## 13. Inbound Dispatch

### 13.1 Static callback trampoline

PubSubClient takes a free-function callback. We use a static trampoline plus a single-instance pointer:

```cpp
// in .cpp
CircuitDigestCloud* CircuitDigestCloud::_instance = nullptr;

void CircuitDigestCloud::_staticCallback(char* topic, uint8_t* payload, unsigned int len) {
    if (_instance) _instance->_onMqttMessage(topic, payload, len);
}
```

`_instance` is set in `begin()`. This means **only one `CircuitDigestCloud` instance is supported per sketch** — adequate for v1.0 and consistent with the device-per-instance MQTT model. Document this clearly in the README.

### 13.2 `_onMqttMessage(topic, payload, len)`

1. Verify topic starts with `_topicBase`. If not, return.
2. Verify the next segment is `/control/`. If not (e.g. echo of our own publishes if broker reflects, which it shouldn't, but defensive), return.
3. Extract `<var>` between `/control/` and `/set` (or `/get`). If `/get`, ignore (it's our own ack echoed by retain or by another subscriber — drop).
4. Null-terminate the variable name into a small scratch buffer (`char varName[33]` or up to a defined max). If too long, drop and log.
5. Parse the JSON payload via `cdParseJson(payload, len, &CDValue, &keyOut)`. If parse fails → drop, set `lastError() = CD_ERR_INVALID_JSON`, log.
6. Look up `<var>` in the registry as a control.
7. If found:
   a. If the variable's type is `CD_AUTO`, lock it to the parsed `CDValue::type()`.
   b. If the variable's type is locked and the parsed type doesn't match → log warning, set `lastError() = CD_ERR_TYPE_MISMATCH`. **Still fire the callback** (permissive: the user may want to handle coercion themselves).
   c. Fire the callback: `cb(varName, value)`.
   d. If effective ack mode for this variable is `CD_ACK_AUTO`, call `_publishAutoAck(varName, value)`.
8. If not found:
   a. If a global fallback callback is registered, fire it.
   b. Always auto-ack the value back (per spec), regardless of fallback presence: build `<base>/control/<var>/get`, build payload echoing the parsed value (as the same type and key), publish QoS 1 retain true. This prevents the dashboard from showing the command as "pending forever" when an unhandled variable arrives.
   c. Log via debug.

### 13.3 `_publishAutoAck(varName, value)` and inbound echo

Builds topic `<base>/control/<var>/get`. Builds payload `{"<var>":<value>}` matching the type carried by `CDValue`:
- `CD_INT` → `cdFormatInt`
- `CD_FLOAT` → `cdFormatFloat`
- `CD_BOOL` → `cdFormatBool`
- `CD_STRING` / `CD_ENUM` → `cdFormatString`
- `CD_AUTO` (parser said null) → suppress ack; log warning.

Publishes QoS 1, retain true.

---

## 14. Debug Logging

When `_debug` is non-null, the library emits human-readable lines to that stream. Every line is prefixed `[CD] `.

Events that are logged (one line each):
- `setCredentials` validation result.
- `begin` start, with cached base topic and username.
- State transitions: `DISCONNECTED → CONNECTING`, `CONNECTING → CONNECTED`, etc.
- Connect attempt result, including PubSubClient state code on failure.
- Subscribe success.
- Online publish (`{"online":1}`).
- Heartbeat publish.
- Outbound sensor publish: variable, type, payload, success/failure.
- Outbound ack publish.
- Inbound control: topic, parsed key/type/value, dispatch result (callback fired / fallback fired / unhandled).
- Type mismatches and JSON parse failures.

Lines are kept short (<120 chars) so they don't dominate Serial bandwidth.

Debug is **off by default**. The README and every example sketch must mention this and show how to enable it (`cd.setDebug(&Serial);`).

---

## 15. Examples

All four examples target ESP32 and ESP8266 in a single sketch using preprocessor selection:

```cpp
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #error "These examples target ESP32 or ESP8266. The library itself supports any Arduino-core board; adapt the WiFi headers accordingly."
#endif
#include <PubSubClient.h>
#include <CircuitDigestCloud.h>
```

Every example must:
- Declare placeholder credentials at the top with `// FILL ME IN` markers.
- Call `cd.setDebug(&Serial);` and include a comment block titled **"Debug logging is OFF by default — comment out the setDebug line for production."**
- Call `Serial.begin(115200);` and wait briefly for Serial.

### 15.1 `01_BasicSensor.ino`

Publishes `temperature` (float) every 5 seconds. Demonstrates: minimal setup, transport handoff, `publishSensor`, no controls.

```cpp
WiFiClient net;
CircuitDigestCloud cd(net);

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(200); }

  cd.setCredentials(MQTT_DEVICE, MQTT_UUID, MQTT_DEVID, MQTT_KEY);
  cd.setDebug(&Serial);                     // remove for production
  cd.registerSensor("temperature", CD_FLOAT);
  cd.begin();
}

void loop() {
  cd.loop();
  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    float t = 24.0f + (millis() % 1000) / 1000.0f;
    cd.publishSensor("temperature", t);
  }
}
```

### 15.2 `02_BasicControl.ino`

Subscribes to a single control `light_1` (bool) with auto-ack, drives an LED on `LED_BUILTIN`. Demonstrates: callback signature, `CDValue::asBool`, auto-ack.

```cpp
void handleLight(const char* var, CDValue v) {
  digitalWrite(LED_BUILTIN, v.asBool() ? HIGH : LOW);
}

void setup() {
  // ... WiFi + cd.setCredentials + cd.setDebug as above ...
  pinMode(LED_BUILTIN, OUTPUT);
  cd.onControl("light_1", handleLight);   // CD_ACK_AUTO by default
  cd.begin();
}

void loop() { cd.loop(); }
```

### 15.3 `03_AllVariableTypes.ino`

Six variables exercising every type, both directions:
- Sensors: `temperature` (float), `count` (int), `presence` (bool), `mode_status` (string/enum).
- Controls: `setpoint` (float, auto-ack), `label` (string, auto-ack).
- Heartbeat interval set to 30s: `cd.setHeartbeatInterval(30);`.
- Includes a comment block on the `CDValue::asString()` lifetime warning and shows copying it into a local buffer.

### 15.4 `04_ManualAckAndManyControls.ino`

Three controls demonstrating manual ack:
- `relay_1` (bool, manual ack) — toggles a GPIO, **reads back** the actual GPIO state, acks with the read-back value.
- `relay_2` (bool, manual ack) — same pattern, separate handler.
- `mode` (string, auto-ack) — shows mixed modes are fine in one sketch.

Also demonstrates the global fallback `cd.onControl(handleUnknown);` and explains the auto-ack-of-unknown behavior in a comment.

---

## 16. Edge Cases & Required Behaviors

1. **Buffer overrun on topic build.** If `_buildTopic*` would exceed `CD_TOPIC_BUFFER_SIZE`, set `lastError() = CD_ERR_TOPIC_TOO_LONG`, log if debug, return without publishing. Variable names producing this should be documented as too long; in practice 32 chars is plenty.

2. **Buffer overrun on payload build.** Same pattern with `CD_ERR_PAYLOAD_TOO_LONG`.

3. **`millis()` rollover** (~49 days). All time arithmetic uses unsigned subtraction (`(uint32_t)(now - last) >= interval`) which wraps correctly.

4. **Network drop mid-publish.** `pubsub.publish` returns false; library sets `CD_ERR_PUBLISH_FAILED`. Next `loop()` will detect `pubsub.connected() == false` and reconnect. The dropped message is **not** retransmitted by the library (publish is fire-and-forget at this layer).

5. **Receiving a command before its variable is registered.** Falls through to global fallback (if any) and the auto-ack-of-unknown path (per requirement). When the user later registers it, future commands go to the per-variable handler.

6. **Same control received twice in quick succession.** Callback fires twice; auto-ack publishes twice. No de-duplication — the dashboard contract treats every `set` as authoritative.

7. **Empty payload on a control topic.** Parse fails; drop; debug-log `CD_ERR_INVALID_JSON`.

8. **Non-JSON payload on a control topic.** Same as above.

9. **JSON with unexpected key (key ≠ variable name in topic).** Use the value parsed from the JSON regardless; the topic-level variable name is authoritative for routing.

10. **`mqtt_devid` containing characters that break topic structure.** Variable names and credential UUIDs are assumed dashboard-validated; the library does no escaping. Document that names should be `[A-Za-z0-9_]+`.

11. **`begin()` called before `setCredentials()`.** Returns false, sets `CD_ERR_BAD_CREDENTIALS`. `loop()` calls do nothing while not initialized.

12. **Calling `publishSensor` from inside an `onControl` callback.** Allowed — PubSubClient handles re-entrant publish from its own callback.

13. **`setHeartbeatInterval(0)` while connected.** Heartbeat stops on the next `loop()` evaluation (no publish until interval > 0 again).

14. **Caller does not provide a connected `Client`.** PubSubClient's `connect()` will fail; library reports failure and backs off. No special handling needed.

15. **Multiple instances.** Not supported in v1.0 (single-instance trampoline). Constructor of a second instance overwrites `_instance`, breaking the first. Document and enforce in a future version with a `setCallback`-style API; for v1.0, the README clearly states "one instance per sketch."

---

## 17. Performance & Footprint Notes (informative)

Indicative figures, not contractual:
- Flash on ESP32: ~10–14 KB (excluding PubSubClient).
- RAM static: ~`160 + 96 + 64 + sizeof(CircuitDigestCloud)` ≈ 400 bytes baseline.
- RAM per registered variable: `sizeof(CDVariable)` ≈ 24 bytes.
- Per-publish heap allocations: zero (all scratch is static).

Not suitable for ATmega328P-class targets in practice — topic buffer alone is 160 bytes, plus PubSubClient's 512 — but the library compiles for them. Document recommended-target list as ESP-class, RP2040, SAMD21+, nRF52, ESP32-* and similar.

---

## 18. README Outline

The README ships with the library and must contain, in this order:
1. Hero line: what the library is.
2. Quick-start: 30-line minimum sketch.
3. Installation: Arduino IDE library manager, manual ZIP, PlatformIO `lib_deps`.
4. Required dependency: PubSubClient.
5. Credentials: where to find `mqtt_device`, `mqtt_uuid`, `mqtt_devid`, `mqtt_key` in the CircuitDigest Cloud dashboard.
6. API reference table (one row per public method).
7. Variable types and the `CDValue` lifetime caveat.
8. Auto-ack vs manual ack.
9. Heartbeat / `state/last_seen` explanation.
10. Reconnect/backoff behavior.
11. Debug logging — opt-in.
12. One-instance-per-sketch limitation.
13. Renaming the library — the four touch points.
14. License + links.

---

## 19. Acceptance Checklist

The library is considered complete when:

- [ ] All public methods in Section 8.2 compile and link on ESP32 (Arduino core 2.x and 3.x), ESP8266 (Arduino core 3.x), RP2040 (earlephilhower core), and AVR (Arduino AVR core) — at minimum compile-clean; runtime tested on ESP32 + ESP8266.
- [ ] LWT is registered with `{"online":0}`, retain true, QoS 1.
- [ ] On every connect, `{"online":1}` is published, retain true, QoS 1.
- [ ] `<base>/control/+/set` is subscribed exactly once per connect at QoS 1.
- [ ] Sensor publish round-trips through the dashboard for all five types.
- [ ] Control receive fires the correct callback; auto-ack appears at `<base>/control/<var>/get` with retain true.
- [ ] Manual ack (`CD_ACK_MANUAL` + `cd.ackControl(...)`) works; auto-ack does not fire when manual is selected.
- [ ] Unknown control variable: auto-ack still fires; global fallback fires if set.
- [ ] Heartbeat publishes empty payload to `<base>/state/last_seen` at the configured cadence.
- [ ] Network drop → exponential backoff reconnect → re-subscribe → re-publish online:1 → heartbeat resumes.
- [ ] `setDebug(&Serial)` produces the events listed in Section 14.
- [ ] All four examples build and run on ESP32 and ESP8266 from a clean checkout.
- [ ] `library.properties`, `library.json`, `keywords.txt`, README, and LICENSE are present.

---

## 20. Open Items (deferred to v1.1+, not blocking v1.0)

These are deliberately **not** part of v1.0:

- TLS support (port 8883). Requires the user to pass `WiFiClientSecure` and handle CA certs; the library itself is already transport-agnostic and does not need changes, but examples and docs should be added.
- Multi-instance support (would require a non-static dispatch mechanism).
- Ack-with-error semantics (e.g. echoing a different value to indicate hardware failure — already possible today via manual ack).
- Persistent ack queue across reboots.
- ArduinoJson backend as a build-time alternative to the internal parser.
- Topic-prefix override for staging/test brokers.

---

*End of document.*
