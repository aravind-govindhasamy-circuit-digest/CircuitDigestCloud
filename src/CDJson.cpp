// Copyright (c) 2026 Jobit Joseph, Circuit Digest
// SPDX-License-Identifier: MIT
#include "CDJson.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static void skipWs(const uint8_t* d, unsigned int len, unsigned int* i) {
    while (*i < len && (d[*i]==' '||d[*i]=='\t'||d[*i]=='\n'||d[*i]=='\r')) (*i)++;
}

static bool readString(const uint8_t* d, unsigned int len, unsigned int* i,
                        char* out, size_t cap) {
    if (*i >= len || d[*i] != '"') return false;
    (*i)++;
    size_t n = 0;
    while (*i < len && d[*i] != '"') {
        char c = (char)d[*i];
        if (c == '\\') {
            (*i)++;
            if (*i >= len) return false;
            char e = (char)d[*i];
            if      (e == '"')  c = '"';
            else if (e == '\\') c = '\\';
            else if (e == 'n')  c = '\n';
            else if (e == 'r')  c = '\r';
            else if (e == 't')  c = '\t';
            else if (e == 'u') {
                // \u00XX
                if (*i + 4 >= len) return false;
                char hex[5]; memcpy(hex, d + *i + 1, 4); hex[4]=0;
                unsigned long cp = strtoul(hex, NULL, 16);
                *i += 4;
                if (n + 1 < cap) out[n++] = (char)(cp & 0xFF);
                (*i)++;
                continue;
            } else c = e;
        }
        if (n + 1 < cap) out[n++] = c;
        (*i)++;
    }
    if (*i >= len) return false;
    (*i)++; // closing "
    out[n] = 0;
    return true;
}

// Parse a JSON value token at *i (after the ':'). Returns true on success.
// null returns false (callers treat null as a non-value).
static bool parseValueToken(const uint8_t* data, unsigned int len, unsigned int* i,
                            CDValue* val, char* strBuf, size_t strBufCap) {
    skipWs(data, len, i);
    if (*i >= len) return false;

    // null
    if (*i + 4 <= len && memcmp(data + *i, "null", 4) == 0) {
        val->_setType(CD_AUTO);
        return false;
    }
    // true
    if (*i + 4 <= len && memcmp(data + *i, "true", 4) == 0) {
        val->_setBool(true); *i += 4; return true;
    }
    // false
    if (*i + 5 <= len && memcmp(data + *i, "false", 5) == 0) {
        val->_setBool(false); *i += 5; return true;
    }
    // string
    if (data[*i] == '"') {
        if (!readString(data, len, i, strBuf, strBufCap)) return false;
        val->_setString(strBuf);
        return true;
    }
    // number
    if (data[*i] == '-' || (data[*i] >= '0' && data[*i] <= '9')) {
        unsigned int start = *i;
        bool isFloat = false;
        if (data[*i] == '-') (*i)++;
        while (*i < len && data[*i] >= '0' && data[*i] <= '9') (*i)++;
        if (*i < len && data[*i] == '.') { isFloat = true; (*i)++; while (*i < len && data[*i] >= '0' && data[*i] <= '9') (*i)++; }
        if (*i < len && (data[*i] == 'e' || data[*i] == 'E')) {
            isFloat = true; (*i)++;
            if (*i < len && (data[*i] == '+' || data[*i] == '-')) (*i)++;
            while (*i < len && data[*i] >= '0' && data[*i] <= '9') (*i)++;
        }
        char nbuf[32]; size_t nlen = *i - start;
        if (nlen >= sizeof(nbuf)) return false;
        memcpy(nbuf, data + start, nlen); nbuf[nlen] = 0;
        if (isFloat) val->_setFloat((float)atof(nbuf));
        else         val->_setInt(atol(nbuf));
        return true;
    }
    return false;
}

bool cdParseJson(const uint8_t* data, unsigned int len,
                 CDValue* val, char* keyOut, size_t keyOutCap,
                 char* strBuf, size_t strBufCap) {
    unsigned int i = 0;
    skipWs(data, len, &i);
    if (i >= len || data[i] != '{') return false;
    i++;
    skipWs(data, len, &i);
    if (!readString(data, len, &i, keyOut, keyOutCap)) return false;
    skipWs(data, len, &i);
    if (i >= len || data[i] != ':') return false;
    i++;
    return parseValueToken(data, len, &i, val, strBuf, strBufCap);
}

bool cdParseField(const uint8_t* data, unsigned int len, const char* field,
                  CDValue* val, char* strBuf, size_t strBufCap) {
    // Build the quoted field token, e.g. "value", and find it in the buffer.
    char needle[40];
    int nn = snprintf(needle, sizeof(needle), "\"%s\"", field);
    if (nn <= 0 || (size_t)nn >= sizeof(needle)) return false;
    size_t needleLen = (size_t)nn;

    for (unsigned int i = 0; i + needleLen <= len; i++) {
        if (memcmp(data + i, needle, needleLen) != 0) continue;
        unsigned int j = i + needleLen;
        skipWs(data, len, &j);
        if (j >= len || data[j] != ':') continue;
        j++;
        return parseValueToken(data, len, &j, val, strBuf, strBufCap);
    }
    return false;
}

size_t cdValueInt(char* out, size_t cap, long v) {
    int n = snprintf(out, cap, "%ld", v);
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
}

size_t cdValueFloat(char* out, size_t cap, float v) {
    // %g gives a compact representation and strips trailing zeros.
    int n = snprintf(out, cap, "%g", v);
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
}

size_t cdValueBool(char* out, size_t cap, bool v) {
    int n = snprintf(out, cap, "%s", v ? "true" : "false");
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
}

#define CD_PUT(c) do { if (pos + 1 >= cap) return 0; out[pos++] = (c); } while(0)

size_t cdValueString(char* out, size_t cap, const char* v) {
    if (!v) v = "";
    size_t pos = 0;
    CD_PUT('"');
    for (const char* p = v; *p; p++) {
        char c = *p;
        if      (c == '"')  { CD_PUT('\\'); CD_PUT('"'); }
        else if (c == '\\') { CD_PUT('\\'); CD_PUT('\\'); }
        else if (c == '\n') { CD_PUT('\\'); CD_PUT('n'); }
        else if (c == '\r') { CD_PUT('\\'); CD_PUT('r'); }
        else if (c == '\t') { CD_PUT('\\'); CD_PUT('t'); }
        else if ((uint8_t)c < 0x20) {
            char esc[7]; snprintf(esc, sizeof(esc), "\\u%04x", (uint8_t)c);
            for (int j = 0; esc[j]; j++) { CD_PUT(esc[j]); }
        } else { CD_PUT(c); }
    }
    CD_PUT('"');
    out[pos] = 0;
    return pos;
}

#undef CD_PUT
