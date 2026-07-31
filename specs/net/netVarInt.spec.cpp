#include <igloo/igloo_alt.h>

#include "../../common/net/netVarInt.h"

using namespace igloo;

Describe(NetVarIntSpec) {

    Describe(RoundTrip) {
        It(should_encode_small_positive_values_in_one_byte) {
            int32_t values[] = {0, 1, 5, 63};
            for (int32_t v : values) {
                uint8_t buf[kNetVarIntMaxBytes];
                int n = NetVarIntPack(buf, sizeof(buf), v);
                Assert::That(n, Equals(1));
                int32_t out = 0;
                int consumed = 0;
                Assert::That(NetVarIntUnpack(buf, n, out, consumed), Equals(true));
                Assert::That(out, Equals(v));
            }
        };
        It(should_encode_small_negative_values_in_one_byte) {
            int32_t values[] = {-1, -5, -63, -64};
            for (int32_t v : values) {
                uint8_t buf[kNetVarIntMaxBytes];
                int n = NetVarIntPack(buf, sizeof(buf), v);
                Assert::That(n, Equals(1));
                int32_t out = 0;
                int consumed = 0;
                Assert::That(NetVarIntUnpack(buf, n, out, consumed), Equals(true));
                Assert::That(out, Equals(v));
            }
        };
        It(should_round_trip_boundary_magnitudes) {
            int32_t values[] = {64, -65, 127, -128, 128, 300, 8191, -8192, 8192,
                                1048575, -1048576, 1048576, INT32_MAX, INT32_MIN};
            for (int32_t v : values) {
                uint8_t buf[kNetVarIntMaxBytes];
                int n = NetVarIntPack(buf, sizeof(buf), v);
                Assert::That(n, Is().GreaterThan(0));
                int32_t out = 0;
                int consumed = 0;
                Assert::That(NetVarIntUnpack(buf, n, out, consumed), Equals(true));
                Assert::That(out, Equals(v));
                Assert::That(consumed, Equals(n));
            }
        };
        It(should_round_trip_random_values) {
            uint32_t seed = 12345;
            for (int i = 0; i < 2000; i++) {
                seed = seed * 1664525u + 1013904223u;
                int32_t v = (int32_t)seed;
                uint8_t buf[kNetVarIntMaxBytes];
                int n = NetVarIntPack(buf, sizeof(buf), v);
                Assert::That(n, Is().GreaterThan(0));
                int32_t out = 0;
                int consumed = 0;
                Assert::That(NetVarIntUnpack(buf, n, out, consumed), Equals(true));
                Assert::That(out, Equals(v));
            }
        };
        It(should_never_need_more_than_five_bytes) {
            uint8_t buf[kNetVarIntMaxBytes];
            Assert::That(NetVarIntPack(buf, sizeof(buf), INT32_MAX), Is().LessThanOrEqualTo(5));
            Assert::That(NetVarIntPack(buf, sizeof(buf), INT32_MIN), Is().LessThanOrEqualTo(5));
        };
    };

    Describe(Errors) {
        It(should_fail_on_a_zero_sized_buffer) {
            Assert::That(NetVarIntPack(nullptr, 0, 42), Equals(0));
        };
        It(should_fail_when_the_buffer_is_too_small) {
            uint8_t buf[1];
            Assert::That(NetVarIntPack(buf, 1, 100000), Equals(0));
        };
        It(should_fail_on_truncated_input) {
            int32_t value = 0;
            int consumed = 0;
            uint8_t truncated[] = {0x80}; // extend bit, but nothing after
            Assert::That(NetVarIntUnpack(truncated, 1, value, consumed), Equals(false));
        };
        It(should_fail_on_more_than_five_bytes) {
            int32_t value = 0;
            int consumed = 0;
            uint8_t overlong[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
            Assert::That(NetVarIntUnpack(overlong, 6, value, consumed), Equals(false));
        };
        It(should_fail_on_empty_input) {
            int32_t value = 0;
            int consumed = 0;
            Assert::That(NetVarIntUnpack(nullptr, 0, value, consumed), Equals(false));
        };
    };
};
