#pragma once

#include <cstdint>

namespace storm {

// ── Variable-length integers ────────────────────────────────────────────────
// Same wire format as Teeworlds: "ESDDDDDD EDDDDDDD EDD..." — the first byte
// carries a sign bit, 6 data bits, and an extend bit; every extend bit adds 7
// more bits. Any int32 packs into at most 5 bytes. Negative values are stored
// bit-complemented.

constexpr int kNetVarIntMaxBytes = 5;

// Packs value into dst. Returns bytes written, or 0 if the buffer is too small.
int NetVarIntPack(uint8_t *dst, int dstSize, int32_t value);

// Unpacks one varint from src. Returns false on truncation, on int32 overflow,
// and on any non-canonical encoding — a value padded out with extend bits is
// rejected even though it decodes, so every value has exactly one valid byte
// string on the wire. NetVarIntPack only ever emits canonical encodings.
// On success, consumed receives the byte count.
bool NetVarIntUnpack(const uint8_t *src, int srcSize, int32_t &value,
                     int &consumed);

} // namespace storm
