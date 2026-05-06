// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#include "CDValue.h"
#include <string.h>

CDValue::CDValue() : _type(CD_AUTO), _s(nullptr) { _v.i = 0; }

CDType      CDValue::type()     const { return _type; }
bool        CDValue::isInt()    const { return _type == CD_INT; }
bool        CDValue::isFloat()  const { return _type == CD_FLOAT; }
bool        CDValue::isBool()   const { return _type == CD_BOOL; }
bool        CDValue::isString() const { return _type == CD_STRING || _type == CD_ENUM; }

long CDValue::asInt() const {
    if (_type == CD_INT)   return _v.i;
    if (_type == CD_FLOAT) return (long)_v.f;
    if (_type == CD_BOOL)  return _v.b ? 1 : 0;
    return 0;
}

float CDValue::asFloat() const {
    if (_type == CD_FLOAT) return _v.f;
    if (_type == CD_INT)   return (float)_v.i;
    if (_type == CD_BOOL)  return _v.b ? 1.0f : 0.0f;
    return 0.0f;
}

bool CDValue::asBool() const {
    if (_type == CD_BOOL)  return _v.b;
    if (_type == CD_INT)   return _v.i != 0;
    if (_type == CD_FLOAT) return _v.f != 0.0f;
    if ((_type == CD_STRING || _type == CD_ENUM) && _s)
        return strcmp(_s, "true") == 0;
    return false;
}

const char* CDValue::asString() const {
    if (_type == CD_STRING || _type == CD_ENUM) return _s;
    return nullptr;
}

void CDValue::_setInt   (long v)        { _type = CD_INT;    _v.i = v; _s = nullptr; }
void CDValue::_setFloat (float v)       { _type = CD_FLOAT;  _v.f = v; _s = nullptr; }
void CDValue::_setBool  (bool v)        { _type = CD_BOOL;   _v.b = v; _s = nullptr; }
void CDValue::_setString(const char* v) { _type = CD_STRING; _s   = v; }
void CDValue::_setType  (CDType t)      { _type = t; }
