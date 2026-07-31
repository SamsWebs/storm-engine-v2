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

    value = result ^ -sign; // two's complement back if negative
    consumed = n + 1;
    return true;
}
