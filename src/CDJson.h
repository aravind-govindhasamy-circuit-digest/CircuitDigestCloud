// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "CDValue.h"

// Parser: fills val and keyOut (null-terminated, max keyOutCap-1 chars).
// Returns true on success. Reads the FIRST key/value of a flat JSON object,
// which is what a flat Anedya valuestore update ({"<slot>": <value>}) looks like.
bool cdParseJson(const uint8_t* data, unsigned int len,
                 CDValue* val, char* keyOut, size_t keyOutCap,
                 char* strBuf, size_t strBufCap);

// Find a named top-level field and parse its value (handles a structured
// valuestore update like {"key":"<slot>","value":<v>,...}). Returns true if the
// field was found and parsed (non-null).
bool cdParseField(const uint8_t* data, unsigned int len, const char* field,
                  CDValue* val, char* strBuf, size_t strBufCap);

// Value-token formatters — emit just the JSON value (no surrounding object), used
// to build the Anedya submitdata payload:
//   {"data":[{"variable":"<slot>","value":<token>,"timestamp":0}]}
// cdValueString includes the surrounding quotes and escapes the contents.
size_t cdValueInt   (char* out, size_t cap, long        v);
size_t cdValueFloat (char* out, size_t cap, float       v);
size_t cdValueBool  (char* out, size_t cap, bool        v);
size_t cdValueString(char* out, size_t cap, const char* v);
