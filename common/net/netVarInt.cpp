#include "netVarInt.h"

int NetVarIntPack(uint8_t *dst, int dstSize, int32_t value) {
    if (dstSize <= 0)
        return 0;

    int i = value;
    if (i < 0) {
        *dst = 0x40; // sign bit
        i = ~i;
    } else {
        *dst = 0;
    }
    *dst |= i & 0x3F; // 6 data bits
    i >>= 6;

    int n = 1;
    while (i != 0) {
        if (n >= dstSize)
            return 0;
        dst[n - 1] |= 0x80; // extend bit
        dst[n] = i & 0x7F;
        i >>= 7;
        n++;
    }
    return n;
}

bool NetVarIntUnpack(const uint8_t *src, int srcSize, int32_t &value, int &consumed) {
    if (srcSize <= 0)
        return false;

    int result = src[0] & 0x3F;
    int sign = (src[0] >> 6) & 1;
    int remaining = srcSize - 1;
    int n = 0;

    while (src[n] & 0x80) {
        n++;
        if (n > 4 || remaining <= 0)
            return false;
        remaining--;
        // The 5th byte only contributes 4 bits (34-bit field, int32 payload).
        result |= (src[n] & (n < 4 ? 0x7F : 0x0F)) << (6 + 7 * (n - 1));
    }

    // Canonical encodings only. NetVarIntPack never emits a trailing zero byte
    // (the loop stops as soon as the value is exhausted) and never sets the
    // 5th byte's top bits, so anything that does is either an overlong encoding
    // of a shorter value or an int32 overflow. Accepting either would make the
    // wire malleable: several byte strings would decode to the same snapshot,
    // which defeats Crc()-based tamper detection and lets a peer pad every
    // varint out to five bytes.
    if (n > 0 && src[n] == 0)
        return false; // overlong: the value fits in n bytes
    if (n == 4 && (src[4] & 0x70) != 0)
        return false; // overflow: bits past the int32 payload

    value = result ^ -sign; // two's complement back if negative
    consumed = n + 1;
    return true;
}
