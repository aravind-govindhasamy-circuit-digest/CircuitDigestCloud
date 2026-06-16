// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#pragma once
#include "CDTypes.h"

// IMPORTANT — asString() returns a pointer into an internal 64-byte buffer that is
// overwritten on the next inbound MQTT message. The pointer is only valid during the
// control callback; if you need the string after the callback returns, copy it:
//   char buf[64]; strlcpy(buf, v.asString(), sizeof(buf));
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
