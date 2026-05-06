// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#pragma once
#include "CDTypes.h"

// asString() is valid only during the callback in which it was delivered.
// Copy it if you need to retain it beyond the callback.
class CDValue {
public:
    CDValue();

    CDType        type()     const;
    bool          isInt()    const;
    bool          isFloat()  const;
    bool          isBool()   const;
    bool          isString() const;

    long          asInt()    const;
    float         asFloat()  const;
    bool          asBool()   const;
    const char*   asString() const;

    void _setInt   (long v);
    void _setFloat (float v);
    void _setBool  (bool v);
    void _setString(const char* v);
    void _setType  (CDType t);

private:
    CDType _type;
    union { long i; float f; bool b; } _v;
    const char* _s;
};
