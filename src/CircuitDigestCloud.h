// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <Client.h>
#include <PubSubClient.h>
#include "CDTypes.h"
#include "CDValue.h"

typedef void (*CDControlCallback)(const char* variable, CDValue value);
typedef void (*CDTransportResetCallback)();

struct CDVariable {
    const char*       name;      // friendly name used in your sketch (e.g. "temperature")
    const char*       slot;      // Anedya slot from the dashboard (e.g. "float0")
    CDDirection       direction;
    CDType            type;
    bool              typeLocked;
    CDControlCallback callback;
    CDAckMode         ackMode;
    CDVariable*       next;
};

// Connects an Arduino-core device to the CircuitDigest Cloud, which is backed by the
// Anedya IoT platform. Telemetry is published to fixed project "slots" (float0..9,
// status0..4); your sketch refers to variables by a friendly name and supplies the
// slot shown on the dashboard. Transport-agnostic — pass a TLS-capable Client
// (e.g. WiFiClientSecure) since Anedya MQTT requires TLS on port 8883.
class CircuitDigestCloud {
public:
    explicit CircuitDigestCloud(Client& transport);
    ~CircuitDigestCloud();

    // Device credentials from the dashboard's device setup panel.
    //   deviceId      — the Physical Device ID (MQTT client id/username; must be the
    //                   device bound to the node, NOT the Anedya node id)
    //   connectionKey — the device's Connection Key (MQTT password)
    bool setCredentials(const char* deviceId, const char* connectionKey);

    // Anedya region (default "ap-in-1"). Sets the broker host to mqtt.<region>.anedya.io.
    void setRegion(const char* region);
    // Override broker host/port directly (takes priority over region).
    void setServer(const char* host, uint16_t port);

    void setBufferSize        (uint16_t bytes);
    void setHeartbeatInterval (uint32_t seconds);
    void setAutoAck           (bool enabled);
    void setDebug             (Stream* stream);
    void setTransportResetCallback(CDTransportResetCallback cb);

    bool begin();
    void loop();

    bool    connected();
    bool    isConnecting();  // true while attempting to connect or in backoff
    void    disconnect();    // cleanly close the MQTT session and reset state
    CDError lastError();

    // Register a sensor variable. `slot` is the Anedya slot from the dashboard
    // (e.g. "float0"). If slot is nullptr, `name` is used as the slot directly.
    bool registerVariable(const char* name, CDType type = CD_AUTO, const char* slot = nullptr);

    // Register a control (output) variable + callback. `slot` as above.
    bool onChange(const char* name, CDControlCallback cb,
                   CDAckMode ack = CD_ACK_AUTO, CDType type = CD_AUTO,
                   const char* slot = nullptr);
    void onChange(CDControlCallback cb);   // global fallback

    bool publishVariable(const char* name, int         value, bool retain = true);
    bool publishVariable(const char* name, long        value, bool retain = true);
    bool publishVariable(const char* name, float       value, bool retain = true);
    bool publishVariable(const char* name, double      value, bool retain = true);
    bool publishVariable(const char* name, bool        value, bool retain = true);
    bool publishVariable(const char* name, const char* value, bool retain = true);

    bool ackChange(const char* name, int         value);
    bool ackChange(const char* name, long        value);
    bool ackChange(const char* name, float       value);
    bool ackChange(const char* name, double      value);
    bool ackChange(const char* name, bool        value);
    bool ackChange(const char* name, const char* value);

private:
    Client&      _transport;
    PubSubClient _pubsub;

    const char* _deviceId;
    const char* _connKey;

    char     _host[64];
    uint16_t _port;

    char*    _topicBase;     // "$anedya/device/<deviceId>"
    uint16_t _topicBaseLen;

    uint16_t _bufferSize;
    uint32_t _heartbeatInterval;   // maps to the MQTT keepalive interval (seconds)
    bool     _autoAck;
    bool     _initialized;
    uint32_t _reqSeq;

    Stream*  _debug;
    CDError  _lastError;
    CDTransportResetCallback _transportReset;

    enum State : uint8_t {
        CD_STATE_DISCONNECTED,
        CD_STATE_BACKOFF,
        CD_STATE_CONNECTING,
        CD_STATE_CONNECTED
    } _state;

    uint32_t _backoffNextMs;
    uint16_t _backoffSeconds;

    CDVariable*       _registryHead;
    CDControlCallback _globalControlCb;

    char _topicScratch  [CD_TOPIC_BUFFER_SIZE];
    char _valueScratch  [CD_PAYLOAD_BUFFER_SIZE];
    char _submitScratch [CD_SUBMIT_BUFFER_SIZE];
    char _inStringBuf   [CD_INBOUND_STRING_BUFFER];

    static CircuitDigestCloud* _instance;
    static void _staticCallback(char* topic, uint8_t* payload, unsigned int len);

    void _onMqttMessage(char* topic, uint8_t* payload, unsigned int len);
    bool _attemptConnect();
    void _onConnected();
    void _scheduleBackoff();

    CDVariable* _findVariable  (const char* name, CDDirection dir);
    CDVariable* _findBySlot    (const char* slot, CDDirection dir);
    CDVariable* _registerVar   (const char* name, CDDirection dir, CDType type, const char* slot);
    const char* _slotOf        (CDVariable* v) const;

    bool _publishSubmit  (const char* slot, const char* valueToken, bool retain);
    bool _doPublishSensor(const char* name, CDType type, const char* valueToken, bool retain);
    void _autoAckValue   (CDVariable* v, CDValue& val);
    void _resetTransport();

    void _logf(const char* fmt, ...);
};
