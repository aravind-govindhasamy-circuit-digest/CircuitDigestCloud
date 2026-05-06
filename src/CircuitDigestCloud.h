// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <Client.h>
#include <PubSubClient.h>
#include "CDTypes.h"
#include "CDValue.h"

typedef void (*CDControlCallback)(const char* variable, CDValue value);

struct CDVariable {
    const char*       name;
    CDDirection       direction;
    CDType            type;
    bool              typeLocked;
    CDControlCallback callback;
    CDAckMode         ackMode;
    CDVariable*       next;
};

class CircuitDigestCloud {
public:
    explicit CircuitDigestCloud(Client& transport);
    ~CircuitDigestCloud();

    bool setCredentials(const char* device, const char* uuid,
                        const char* devid,  const char* key);
    void setBufferSize        (uint16_t bytes);
    void setHeartbeatInterval (uint32_t seconds);
    void setAutoAck           (bool enabled);
    void setDebug             (Stream* stream);

    bool begin();
    void loop();

    bool    connected();
    CDError lastError();

    bool registerSensor(const char* name, CDType type = CD_AUTO);
    bool onControl(const char* name, CDControlCallback cb,
                   CDAckMode ack = CD_ACK_AUTO, CDType type = CD_AUTO);
    void onControl(CDControlCallback cb);   // global fallback

    bool publishSensor(const char* name, int         value);
    bool publishSensor(const char* name, long        value);
    bool publishSensor(const char* name, float       value);
    bool publishSensor(const char* name, double      value);
    bool publishSensor(const char* name, bool        value);
    bool publishSensor(const char* name, const char* value);

    bool ackControl(const char* name, int         value);
    bool ackControl(const char* name, long        value);
    bool ackControl(const char* name, float       value);
    bool ackControl(const char* name, double      value);
    bool ackControl(const char* name, bool        value);
    bool ackControl(const char* name, const char* value);

private:
    Client&      _transport;
    PubSubClient _pubsub;

    const char* _credDevice;
    const char* _credUuid;
    const char* _credDevid;
    const char* _credKey;

    char*    _topicBase;
    char*    _username;
    uint16_t _topicBaseLen;

    uint16_t _bufferSize;
    uint32_t _heartbeatInterval;
    uint32_t _lastHeartbeatMs;
    bool     _autoAck;
    bool     _initialized;

    Stream*  _debug;
    CDError  _lastError;

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
    char _payloadScratch[CD_PAYLOAD_BUFFER_SIZE];
    char _inStringBuf   [CD_INBOUND_STRING_BUFFER];

    static CircuitDigestCloud* _instance;
    static void _staticCallback(char* topic, uint8_t* payload, unsigned int len);

    void _onMqttMessage(char* topic, uint8_t* payload, unsigned int len);
    bool _attemptConnect();
    void _onConnected();
    void _publishOnline(bool online);
    void _publishHeartbeat();
    void _scheduleBackoff();

    CDVariable* _findVariable  (const char* name, CDDirection dir);
    CDVariable* _registerVar   (const char* name, CDDirection dir, CDType type);

    bool   _publishRaw   (const char* topic, const char* payload, bool retain);
    size_t _buildSensor  (const char* var, char* out, size_t cap);
    size_t _buildCtlGet  (const char* var, char* out, size_t cap);
    size_t _buildState   (const char* leaf, char* out, size_t cap);

    bool _doPublishSensor(const char* name, CDType type, const char* payload);
    bool _doAckControl   (const char* name, const char* payload);
    void _autoAckValue   (const char* varName, CDValue& val);

    void _logf(const char* fmt, ...);
};
