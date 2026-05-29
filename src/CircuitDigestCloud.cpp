// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#include "CircuitDigestCloud.h"
#include "CDJson.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static const char CD_MQTT_HOST[]       = "mqtt.circuitdigest.cloud";
static const uint16_t CD_MQTT_PORT     = 1883;
static const char CD_USERNAME_PREFIX[] = "mqtt_u_";
static const char CD_TOPIC_ROOT[]      = "cd/users/";
static const char CD_TOPIC_DEVICES[]   = "/devices/";
static const char CD_MQTT_CLIENT_ID[]  = "CircuitDigestCloudDevice";

CircuitDigestCloud* CircuitDigestCloud::_instance = nullptr;

// ---- Construction ----------------------------------------------------------

CircuitDigestCloud::CircuitDigestCloud(Client& transport)
    : _transport(transport), _pubsub(transport),
      _credUuid(nullptr),
      _credDevid(nullptr), _credKey(nullptr),
      _topicBase(nullptr), _username(nullptr), _topicBaseLen(0),
      _bufferSize(CD_DEFAULT_BUFFER_SIZE),
      _heartbeatInterval(CD_DEFAULT_HEARTBEAT_S),
      _lastHeartbeatMs(0), _autoAck(true), _initialized(false),
      _debug(nullptr), _lastError(CD_OK),
      _state(CD_STATE_DISCONNECTED),
      _backoffNextMs(0), _backoffSeconds(1),
      _registryHead(nullptr), _globalControlCb(nullptr)
{
    _instance = this;
}

CircuitDigestCloud::~CircuitDigestCloud() {
    free(_topicBase);
    free(_username);
    CDVariable* v = _registryHead;
    while (v) { CDVariable* n = v->next; delete v; v = n; }
    if (_instance == this) _instance = nullptr;
}

// ---- Configuration ---------------------------------------------------------

bool CircuitDigestCloud::setCredentials(const char* uuid,
                                        const char* devid,  const char* key) {
    _lastError = CD_OK;
    if (!uuid || !*uuid ||
        !devid  || !*devid  || !key  || !*key) {
        _lastError = CD_ERR_BAD_CREDENTIALS;
        _logf("[CD] setCredentials: missing field");
        return false;
    }
    _credUuid = uuid;
    _credDevid  = devid;  _credKey  = key;
    _logf("[CD] setCredentials: OK");
    return true;
}

void CircuitDigestCloud::setBufferSize(uint16_t bytes) {
    _bufferSize = (bytes < 256) ? 256 : bytes;
}

void CircuitDigestCloud::setHeartbeatInterval(uint32_t seconds) {
    _heartbeatInterval = seconds;
}

void CircuitDigestCloud::setAutoAck(bool enabled) { _autoAck = enabled; }

void CircuitDigestCloud::setDebug(Stream* stream) { _debug = stream; }

// ---- begin() ---------------------------------------------------------------

bool CircuitDigestCloud::begin() {
    _lastError = CD_OK;
    if (!_credUuid || !_credDevid || !_credKey) {
        _lastError = CD_ERR_BAD_CREDENTIALS;
        return false;
    }
    free(_topicBase); _topicBase = nullptr;
    free(_username);  _username  = nullptr;

    // Build topic base
    size_t blen = strlen(CD_TOPIC_ROOT) + strlen(_credUuid) +
                  strlen(CD_TOPIC_DEVICES) + strlen(_credDevid) + 1;
    _topicBase = (char*)malloc(blen);
    if (!_topicBase) { _lastError = CD_ERR_OUT_OF_MEMORY; return false; }
    snprintf(_topicBase, blen, "%s%s%s%s",
             CD_TOPIC_ROOT, _credUuid, CD_TOPIC_DEVICES, _credDevid);
    _topicBaseLen = (uint16_t)(blen - 1);

    // Build username
    size_t ulen = strlen(CD_USERNAME_PREFIX) + strlen(_credDevid) + 1;
    _username = (char*)malloc(ulen);
    if (!_username) { free(_topicBase); _topicBase=nullptr; _lastError=CD_ERR_OUT_OF_MEMORY; return false; }
    snprintf(_username, ulen, "%s%s", CD_USERNAME_PREFIX, _credDevid);

    _pubsub.setServer(CD_MQTT_HOST, CD_MQTT_PORT);
    _pubsub.setBufferSize(_bufferSize);
    _pubsub.setCallback(_staticCallback);

    _state = CD_STATE_DISCONNECTED;
    _backoffSeconds = 1;
    _backoffNextMs  = 0;
    _lastHeartbeatMs = 0;
    _initialized = true;

    _logf("[CD] begin: base=%s user=%s", _topicBase, _username);
    return true;
}

// ---- loop() ----------------------------------------------------------------

void CircuitDigestCloud::loop() {
    if (!_initialized) return;

    if (_state == CD_STATE_CONNECTED && !_pubsub.connected()) {
        _logf("[CD] connection lost → DISCONNECTED");
        _state = CD_STATE_DISCONNECTED;
    }

    if (_state == CD_STATE_DISCONNECTED) {
        _state = CD_STATE_CONNECTING;
        if (!_attemptConnect()) {
            _scheduleBackoff();
            _state = CD_STATE_BACKOFF;
        }
    } else if (_state == CD_STATE_BACKOFF) {
        if ((uint32_t)(millis() - _backoffNextMs) < 0x80000000UL) {
            _state = CD_STATE_CONNECTING;
            if (!_attemptConnect()) {
                _scheduleBackoff();
                _state = CD_STATE_BACKOFF;
            }
        }
    } else if (_state == CD_STATE_CONNECTED) {
        _pubsub.loop();
        if (_heartbeatInterval > 0) {
            uint32_t now = millis();
            if (_lastHeartbeatMs == 0 ||
                (uint32_t)(now - _lastHeartbeatMs) >= _heartbeatInterval * 1000UL) {
                _publishHeartbeat();
                _lastHeartbeatMs = now;
            }
        }
    }
}

// ---- Connection ------------------------------------------------------------

bool CircuitDigestCloud::connected() { return _pubsub.connected(); }

CDError CircuitDigestCloud::lastError() { return _lastError; }

bool CircuitDigestCloud::_attemptConnect() {
    char lwtTopic[CD_TOPIC_BUFFER_SIZE];
    if (!_buildState("online", lwtTopic, sizeof(lwtTopic))) return false;

    _logf("[CD] connecting as %s ...", CD_MQTT_CLIENT_ID);
    bool ok = _pubsub.connect(CD_MQTT_CLIENT_ID, _username, _credKey,
                              lwtTopic, 1, true, "{\"online\":0}", true);
    if (ok) {
        _state = CD_STATE_CONNECTED;
        _onConnected();
        _logf("[CD] connected");
    } else {
        _logf("[CD] connect failed (state=%d)", _pubsub.state());
    }
    return ok;
}

void CircuitDigestCloud::_onConnected() {
    char sub[CD_TOPIC_BUFFER_SIZE];
    snprintf(sub, sizeof(sub), "%s/control/+/set", _topicBase);
    _pubsub.subscribe(sub, 1);
    _logf("[CD] subscribed %s", sub);
    _publishOnline(true);
    _lastHeartbeatMs = 0;
    _backoffSeconds  = 1;
}

void CircuitDigestCloud::_publishOnline(bool online) {
    char topic[CD_TOPIC_BUFFER_SIZE];
    if (!_buildState("online", topic, sizeof(topic))) return;
    char payload[20];
    snprintf(payload, sizeof(payload), "{\"online\":%d}", online ? 1 : 0);
    _pubsub.publish(topic, (const uint8_t*)payload, strlen(payload), true);
    _logf("[CD] online → %s", payload);
}

void CircuitDigestCloud::_publishHeartbeat() {
    char topic[CD_TOPIC_BUFFER_SIZE];
    if (!_buildState("last_seen", topic, sizeof(topic))) return;
    _pubsub.publish(topic, (const uint8_t*)"", 0, false);
    _logf("[CD] heartbeat");
}

void CircuitDigestCloud::_scheduleBackoff() {
    _backoffNextMs = millis() + (uint32_t)_backoffSeconds * 1000UL;
    _logf("[CD] backoff %us", _backoffSeconds);
    _backoffSeconds = (_backoffSeconds * 2 > CD_BACKOFF_MAX_S)
                      ? CD_BACKOFF_MAX_S : _backoffSeconds * 2;
}

// ---- Registry --------------------------------------------------------------

CDVariable* CircuitDigestCloud::_findVariable(const char* name, CDDirection dir) {
    for (CDVariable* v = _registryHead; v; v = v->next)
        if (v->direction == dir && strcmp(v->name, name) == 0) return v;
    return nullptr;
}

CDVariable* CircuitDigestCloud::_registerVar(const char* name, CDDirection dir, CDType type) {
    CDVariable* v = _findVariable(name, dir);
    if (v) return v;
    v = new CDVariable();
    if (!v) { _lastError = CD_ERR_OUT_OF_MEMORY; return nullptr; }
    v->name       = name;
    v->direction  = dir;
    v->type       = type;
    v->typeLocked = (type != CD_AUTO);
    v->callback   = nullptr;
    v->ackMode    = CD_ACK_AUTO;
    v->next       = _registryHead;
    _registryHead = v;
    return v;
}

bool CircuitDigestCloud::registerVariable(const char* name, CDType type) {
    _lastError = CD_OK;
    return _registerVar(name, CD_DIR_SENSOR, type) != nullptr;
}

bool CircuitDigestCloud::onChange(const char* name, CDControlCallback cb,
                                   CDAckMode ack, CDType type) {
    _lastError = CD_OK;
    if (!name || !cb) { _lastError = CD_ERR_BAD_CREDENTIALS; return false; }
    CDVariable* v = _registerVar(name, CD_DIR_CONTROL, type);
    if (!v) return false;
    v->callback   = cb;
    v->ackMode    = ack;
    v->type       = type;
    v->typeLocked = (type != CD_AUTO);
    return true;
}

void CircuitDigestCloud::onChange(CDControlCallback cb) {
    _globalControlCb = cb;
}

// ---- Topic builders --------------------------------------------------------

size_t CircuitDigestCloud::_buildSensor(const char* var, char* out, size_t cap) {
    int n = snprintf(out, cap, "%s/sensor/%s", _topicBase, var);
    if (n <= 0 || (size_t)n >= cap) { _lastError = CD_ERR_TOPIC_TOO_LONG; return 0; }
    return (size_t)n;
}

size_t CircuitDigestCloud::_buildCtlGet(const char* var, char* out, size_t cap) {
    int n = snprintf(out, cap, "%s/control/%s/get", _topicBase, var);
    if (n <= 0 || (size_t)n >= cap) { _lastError = CD_ERR_TOPIC_TOO_LONG; return 0; }
    return (size_t)n;
}

size_t CircuitDigestCloud::_buildState(const char* leaf, char* out, size_t cap) {
    int n = snprintf(out, cap, "%s/state/%s", _topicBase, leaf);
    if (n <= 0 || (size_t)n >= cap) { _lastError = CD_ERR_TOPIC_TOO_LONG; return 0; }
    return (size_t)n;
}

// ---- Publish helpers -------------------------------------------------------

bool CircuitDigestCloud::_publishRaw(const char* topic, const char* payload, bool retain) {
    bool ok = _pubsub.publish(topic, (const uint8_t*)payload, strlen(payload), retain);
    if (!ok) _lastError = CD_ERR_PUBLISH_FAILED;
    return ok;
}

bool CircuitDigestCloud::_doPublishSensor(const char* name, CDType type, const char* payload, bool retain) {
    if (!_pubsub.connected()) { _lastError = CD_ERR_NOT_CONNECTED; return false; }
    CDVariable* v = _findVariable(name, CD_DIR_SENSOR);
    if (!v) v = _registerVar(name, CD_DIR_SENSOR, type);
    if (v && !v->typeLocked) { v->type = type; v->typeLocked = true; }
    if (!_buildSensor(name, _topicScratch, sizeof(_topicScratch))) return false;
    if (!payload || strlen(payload) == 0) { _lastError = CD_ERR_PAYLOAD_TOO_LONG; return false; }
    bool ok = _publishRaw(_topicScratch, payload, retain);
    _logf("[CD] sensor %s → %s [%s]", name, payload, ok ? "ok" : "fail");
    return ok;
}

bool CircuitDigestCloud::_doAckControl(const char* name, const char* payload) {
    if (!_pubsub.connected()) { _lastError = CD_ERR_NOT_CONNECTED; return false; }
    if (!_findVariable(name, CD_DIR_CONTROL) && !_globalControlCb) {
        _lastError = CD_ERR_UNKNOWN_VARIABLE; return false;
    }
    if (!_buildCtlGet(name, _topicScratch, sizeof(_topicScratch))) return false;
    bool ok = _publishRaw(_topicScratch, payload, true);
    _logf("[CD] ack %s → %s [%s]", name, payload, ok ? "ok" : "fail");
    return ok;
}

// ---- publishSensor overloads -----------------------------------------------

bool CircuitDigestCloud::publishSensor(const char* name, long value, bool retain) {
    _lastError = CD_OK;
    if (!cdFormatInt(_payloadScratch, sizeof(_payloadScratch), name, value))
        { _lastError = CD_ERR_PAYLOAD_TOO_LONG; return false; }
    return _doPublishSensor(name, CD_INT, _payloadScratch, retain);
}
bool CircuitDigestCloud::publishSensor(const char* name, int value, bool retain) {
    return publishSensor(name, (long)value, retain);
}
bool CircuitDigestCloud::publishSensor(const char* name, float value, bool retain) {
    _lastError = CD_OK;
    if (!cdFormatFloat(_payloadScratch, sizeof(_payloadScratch), name, value))
        { _lastError = CD_ERR_PAYLOAD_TOO_LONG; return false; }
    return _doPublishSensor(name, CD_FLOAT, _payloadScratch, retain);
}
bool CircuitDigestCloud::publishSensor(const char* name, double value, bool retain) {
    return publishSensor(name, (float)value, retain);
}
bool CircuitDigestCloud::publishSensor(const char* name, bool value, bool retain) {
    _lastError = CD_OK;
    if (!cdFormatBool(_payloadScratch, sizeof(_payloadScratch), name, value))
        { _lastError = CD_ERR_PAYLOAD_TOO_LONG; return false; }
    return _doPublishSensor(name, CD_BOOL, _payloadScratch, retain);
}
bool CircuitDigestCloud::publishSensor(const char* name, const char* value, bool retain) {
    _lastError = CD_OK;
    if (!cdFormatString(_payloadScratch, sizeof(_payloadScratch), name, value))
        { _lastError = CD_ERR_PAYLOAD_TOO_LONG; return false; }
    return _doPublishSensor(name, CD_STRING, _payloadScratch, retain);
}

// ---- ackControl overloads --------------------------------------------------

bool CircuitDigestCloud::ackControl(const char* name, long value) {
    _lastError = CD_OK;
    if (!cdFormatInt(_payloadScratch, sizeof(_payloadScratch), name, value))
        { _lastError = CD_ERR_PAYLOAD_TOO_LONG; return false; }
    return _doAckControl(name, _payloadScratch);
}
bool CircuitDigestCloud::ackControl(const char* name, int value) {
    return ackControl(name, (long)value);
}
bool CircuitDigestCloud::ackControl(const char* name, float value) {
    _lastError = CD_OK;
    if (!cdFormatFloat(_payloadScratch, sizeof(_payloadScratch), name, value))
        { _lastError = CD_ERR_PAYLOAD_TOO_LONG; return false; }
    return _doAckControl(name, _payloadScratch);
}
bool CircuitDigestCloud::ackControl(const char* name, double value) {
    return ackControl(name, (float)value);
}
bool CircuitDigestCloud::ackControl(const char* name, bool value) {
    _lastError = CD_OK;
    if (!cdFormatBool(_payloadScratch, sizeof(_payloadScratch), name, value))
        { _lastError = CD_ERR_PAYLOAD_TOO_LONG; return false; }
    return _doAckControl(name, _payloadScratch);
}
bool CircuitDigestCloud::ackControl(const char* name, const char* value) {
    _lastError = CD_OK;
    if (!cdFormatString(_payloadScratch, sizeof(_payloadScratch), name, value))
        { _lastError = CD_ERR_PAYLOAD_TOO_LONG; return false; }
    return _doAckControl(name, _payloadScratch);
}

// ---- Inbound dispatch ------------------------------------------------------

void CircuitDigestCloud::_staticCallback(char* topic, uint8_t* payload, unsigned int len) {
    if (_instance) _instance->_onMqttMessage(topic, payload, len);
}

void CircuitDigestCloud::_onMqttMessage(char* topic, uint8_t* payload, unsigned int len) {
    // Must start with base
    if (strncmp(topic, _topicBase, _topicBaseLen) != 0) return;
    const char* rest = topic + _topicBaseLen;

    // Must be /control/
    if (strncmp(rest, "/control/", 9) != 0) return;
    const char* varStart = rest + 9;

    // Extract variable name (up to /set or /get)
    const char* slash = strchr(varStart, '/');
    if (!slash) return;
    size_t vlen = (size_t)(slash - varStart);

    // Drop /get echoes
    if (strcmp(slash, "/get") == 0) return;
    if (strcmp(slash, "/set") != 0) return;

    char varName[33];
    if (vlen >= sizeof(varName)) {
        _logf("[CD] variable name too long, dropping");
        return;
    }
    memcpy(varName, varStart, vlen); varName[vlen] = 0;

    // Parse JSON
    char keyOut[33];
    CDValue val;
    if (!cdParseJson(payload, len, &val, keyOut, sizeof(keyOut),
                     _inStringBuf, sizeof(_inStringBuf))) {
        _lastError = CD_ERR_INVALID_JSON;
        _logf("[CD] parse failed: %s", varName);
        return;
    }

    _logf("[CD] control %s type=%d", varName, (int)val.type());

    CDVariable* v = _findVariable(varName, CD_DIR_CONTROL);
    if (v) {
        if (v->type == CD_AUTO && !v->typeLocked) {
            v->type = val.type(); v->typeLocked = true;
        } else if (v->typeLocked && v->type != val.type() &&
                   !(v->type == CD_ENUM && val.type() == CD_STRING)) {
            _lastError = CD_ERR_TYPE_MISMATCH;
            _logf("[CD] type mismatch %s expected=%d got=%d",
                  varName, (int)v->type, (int)val.type());
        }
        v->callback(varName, val);
        bool doAuto = (v->ackMode == CD_ACK_AUTO) ||
                      (v->ackMode != CD_ACK_MANUAL && _autoAck);
        if (doAuto) _autoAckValue(varName, val);
    } else {
        if (_globalControlCb) _globalControlCb(varName, val);
        // always ack unknowns
        _autoAckValue(varName, val);
        _logf("[CD] unknown control %s, acked", varName);
    }
}

void CircuitDigestCloud::_autoAckValue(const char* varName, CDValue& val) {
    size_t n = 0;
    switch (val.type()) {
        case CD_INT:    n = cdFormatInt   (_payloadScratch, sizeof(_payloadScratch), varName, val.asInt());    break;
        case CD_FLOAT:  n = cdFormatFloat (_payloadScratch, sizeof(_payloadScratch), varName, val.asFloat());  break;
        case CD_BOOL:   n = cdFormatBool  (_payloadScratch, sizeof(_payloadScratch), varName, val.asBool());   break;
        case CD_STRING:
        case CD_ENUM:   n = cdFormatString(_payloadScratch, sizeof(_payloadScratch), varName, val.asString()); break;
        default:
            _logf("[CD] autoAck: null value suppressed for %s", varName);
            return;
    }
    if (!n) return;
    if (!_buildCtlGet(varName, _topicScratch, sizeof(_topicScratch))) return;
    _pubsub.publish(_topicScratch, (const uint8_t*)_payloadScratch, n, true);
}

// ---- Debug -----------------------------------------------------------------

void CircuitDigestCloud::_logf(const char* fmt, ...) {
    if (!_debug) return;
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    _debug->println(buf);
}
