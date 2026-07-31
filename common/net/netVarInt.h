#pragma once

#include <cstdint>

// ── Variable-length integers ────────────────────────────────────────────────
// Same wire format as Teeworlds: "ESDDDDDD EDDDDDDD EDD..." — the first byte
// carries a sign bit, 6 data bits, and an extend bit; every extend bit adds 7
// more bits. Any int32 packs into at most 5 bytes. Negative values are stored
// bit-complemented.

constexpr int kNetVarIntMaxBytes = 5;

// Packs value into dst. Returns bytes written, or 0 if the buffer is too small.
int NetVarIntPack(uint8_t *dst, int dstSize, int32_t value);

// Unpacks one varint from src. Returns false on truncation or overflow.
// On success, consumed receives the byte count.
bool NetVarIntUnpack(const uint8_t *src, int srcSize, int32_t &value, int &consumed);
