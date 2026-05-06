// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "CDValue.h"

// Parser: fills val and keyOut (null-terminated, max keyOutCap-1 chars).
// Returns true on success.
bool cdParseJson(const uint8_t* data, unsigned int len,
                 CDValue* val, char* keyOut, size_t keyOutCap,
                 char* strBuf, size_t strBufCap);

size_t cdFormatInt   (char* out, size_t cap, const char* key, long        v);
size_t cdFormatFloat (char* out, size_t cap, const char* key, float       v);
size_t cdFormatBool  (char* out, size_t cap, const char* key, bool        v);
size_t cdFormatString(char* out, size_t cap, const char* key, const char* v);
