#include <cstring>
#include <set>
#include <string>

#include <igloo/igloo_alt.h>

#include "../../common/net/netPacket.h"

using namespace igloo;

Describe(NetPacketSpec) {

  Describe(ChunkHeader) {
    It(should_round_trip_a_plain_chunk) {
      NetChunkHeader h;
      h.size = 42;
      h.flags = 0;
      h.sequence = 0;
      uint8_t buf[4];
      Assert::That(h.Pack(buf), Equals(true));
      NetChunkHeader out;
      int consumed = 0;
      Assert::That(out.Unpack(buf, 4, consumed), Equals(true));
      Assert::That(out.size, Equals(42));
      Assert::That(out.flags, Equals(0));
      Assert::That(consumed, Equals(2));
    };
    It(should_round_trip_a_vital_chunk_with_sequence) {
      NetChunkHeader h;
      h.size = 4095;
      h.flags = kNetChunkVital;
      h.sequence = 1023;
      uint8_t buf[4];
      Assert::That(h.Pack(buf), Equals(true));
      NetChunkHeader out;
      int consumed = 0;
      Assert::That(out.Unpack(buf, 4, consumed), Equals(true));
      Assert::That(out.size, Equals(4095));
      Assert::That(out.flags, Equals(kNetChunkVital));
      Assert::That(out.sequence, Equals(1023));
      Assert::That(consumed, Equals(3));
    };
    It(should_round_trip_all_sequence_values) {
      for (int seq = 0; seq < 1024; seq++) {
        NetChunkHeader h;
        h.size = 1;
        h.flags = kNetChunkVital;
        h.sequence = seq;
        uint8_t buf[4];
        Assert::That(h.Pack(buf), Equals(true));
        NetChunkHeader out;
        int consumed = 0;
        Assert::That(out.Unpack(buf, 4, consumed), Equals(true));
        Assert::That(out.sequence, Equals(seq));
      }
    };
    It(should_reject_sizes_over_4095) {
      NetChunkHeader h;
      h.size = 4096;
      uint8_t buf[4];
      Assert::That(h.Pack(buf), Equals(false));
    };
    It(should_reject_truncated_headers) {
      NetChunkHeader out;
      int consumed = 0;
      uint8_t partial[] = {0x40}; // vital flag set, 1 byte only
      Assert::That(out.Unpack(partial, 1, consumed), Equals(false));
    };
  };

  Describe(PacketHeader) {
    It(should_round_trip_flags_ack_chunks_and_token) {
      uint8_t buf[7];
      NetPacketHeaderPack(buf, kNetPacketResend, 0x2FF, 250, 0xA1B2C3D4);
      int flags = 0, ack = 0, numChunks = 0;
      uint32_t token = 0;
      Assert::That(NetPacketHeaderUnpack(buf, 7, flags, ack, numChunks, token),
                   Equals(true));
      Assert::That(flags, Equals(kNetPacketResend));
      Assert::That(ack, Equals(0x2FF));
      Assert::That(numChunks, Equals(250));
      Assert::That(token, Equals(0xA1B2C3D4));
    };
    It(should_round_trip_all_ack_values) {
      for (int ack = 0; ack < 1024; ack++) {
        uint8_t buf[7];
        NetPacketHeaderPack(buf, 0, ack, 0, 0);
        int flags = 0, outAck = 0, numChunks = 0;
        uint32_t token = 0;
        Assert::That(
            NetPacketHeaderUnpack(buf, 7, flags, outAck, numChunks, token),
            Equals(true));
        Assert::That(outAck, Equals(ack));
      }
    };
    It(should_reject_truncated_packets) {
      int flags = 0, ack = 0, numChunks = 0;
      uint32_t token = 0;
      uint8_t shortPacket[6] = {};
      Assert::That(
          NetPacketHeaderUnpack(shortPacket, 6, flags, ack, numChunks, token),
          Equals(false));
    };
  };

  Describe(ControlPackets) {
    It(should_mark_a_control_datagram) {
      uint8_t buf[16] = {};
      buf[0] = kNetControlMagic;
      Assert::That(NetControlPacket::IsControl(buf, 2), Equals(true));
    };
    It(should_not_mark_a_connected_packet) {
      // Connected packets start with flags << 2 | ackHigh, max 0x07.
      for (int byte0 = 0; byte0 <= 0x07; byte0++) {
        uint8_t buf[2] = {(uint8_t)byte0, 0};
        Assert::That(NetControlPacket::IsControl(buf, 2), Equals(false));
      }
    };
    It(should_round_trip_a_connect_message_with_nonce) {
      NetControlPacket ctrl;
      ctrl.message = kNetControlConnect;
      uint8_t nonce[4] = {0xDE, 0xAD, 0xBE, 0xEF};
      std::memcpy(ctrl.payload, nonce, 4);
      ctrl.payloadSize = 4;
      uint8_t buf[32];
      int size = 0;
      Assert::That(ctrl.Pack(buf, sizeof(buf), size), Equals(true));
      Assert::That(size, Equals(6));
      Assert::That(NetControlPacket::IsControl(buf, size), Equals(true));
      NetControlPacket out;
      Assert::That(out.Unpack(buf, size), Equals(true));
      Assert::That(out.message, Equals(kNetControlConnect));
      Assert::That(out.payloadSize, Equals(4));
      Assert::That(std::memcmp(out.payload, nonce, 4), Equals(0));
    };
    It(should_reject_short_datagrams) {
      NetControlPacket out;
      uint8_t oneByte[] = {kNetControlMagic};
      Assert::That(out.Unpack(oneByte, 1), Equals(false));
    };
    It(should_reject_datagrams_larger_than_the_payload) {
      // A full-MTU control datagram must not overrun the 1393-byte
      // payload: Unpack used to assign payloadSize = size - 2 before
      // bounding it, so a 1400-byte datagram read 1398 bytes out of a
      // 1393-byte array (and landed 5 bytes past the stack object).
      NetControlPacket out;
      uint8_t full[kNetMaxPacketSize] = {};
      std::memset(full, kNetControlMagic, sizeof(full));
      Assert::That(out.Unpack(full, kNetMaxPayload + 3), Equals(false));
      Assert::That(out.Unpack(full, kNetMaxPacketSize), Equals(false));
      Assert::That(out.payloadSize, Is().LessThanOrEqualTo(kNetMaxPayload));
    };
    It(should_accept_a_payload_of_exactly_the_maximum) {
      NetControlPacket out;
      uint8_t full[kNetMaxPacketSize] = {};
      std::memset(full, kNetControlMagic, sizeof(full));
      Assert::That(out.Unpack(full, kNetMaxPayload + 2), Equals(true));
      Assert::That(out.payloadSize, Equals(kNetMaxPayload));
    };
  };

  Describe(MessageWriterReader) {
    It(should_round_trip_ints_strings_and_raw) {
      NetMessageWriter w;
      Assert::That(w.WriteInt(-12345), Equals(true));
      Assert::That(w.WriteString("hello world"), Equals(true));
      uint8_t raw[] = {1, 2, 3, 4};
      Assert::That(w.WriteRaw(raw, 4), Equals(true));

      NetMessageReader r(w.Data(), w.Size());
      int32_t i = 0;
      Assert::That(r.ReadInt(i), Equals(true));
      Assert::That(i, Equals(-12345));
      char str[32];
      Assert::That(r.ReadString(str, sizeof(str)), Equals(true));
      Assert::That(std::string(str), Equals("hello world"));
      uint8_t outRaw[4] = {};
      Assert::That(r.ReadRaw(outRaw, 4), Equals(true));
      Assert::That(std::memcmp(outRaw, raw, 4), Equals(0));
      Assert::That(r.Finished(), Equals(true));
    };
    It(should_fail_reading_past_the_end) {
      NetMessageWriter w;
      w.WriteInt(7);
      NetMessageReader r(w.Data(), w.Size());
      int32_t i = 0;
      Assert::That(r.ReadInt(i), Equals(true));
      Assert::That(r.ReadInt(i), Equals(false));
    };
    It(should_reject_overlong_strings) {
      NetMessageWriter w;
      Assert::That(w.WriteString("toolong"), Equals(true));
      NetMessageReader r(w.Data(), w.Size());
      char small[4];
      Assert::That(r.ReadString(small, 4), Equals(false));
    };
    It(should_reject_writes_that_overflow_the_buffer) {
      NetMessageWriter w;
      Assert::That(w.WriteRaw(nullptr, w.kMaxSize + 1), Equals(false));
    };
    It(should_preserve_empty_strings) {
      NetMessageWriter w;
      w.WriteString("");
      NetMessageReader r(w.Data(), w.Size());
      char str[8] = "xxxx";
      Assert::That(r.ReadString(str, sizeof(str)), Equals(true));
      Assert::That(std::string(str), Equals(""));
    };
    It(should_reject_a_wire_string_with_no_terminator) {
      // Hand-built: a length of 5 followed by five bytes and no NUL. The old
      // reader terminated at out[outSize - 1] instead, so the caller got its
      // own stale buffer bytes back as if the peer had sent them.
      NetMessageWriter w;
      Assert::That(w.WriteInt(5), Equals(true));
      Assert::That(w.WriteRaw("ABCDE", 5), Equals(true));

      NetMessageReader r(w.Data(), w.Size());
      char out[16];
      std::memset(out, 'Z', sizeof(out));
      Assert::That(r.ReadString(out, sizeof(out)), Equals(false));
      Assert::That(std::strlen(out), Equals((size_t)0));
    };
    It(should_reject_an_unterminated_string_that_fills_the_buffer) {
      NetMessageWriter w;
      Assert::That(w.WriteInt(4), Equals(true));
      Assert::That(w.WriteRaw("ABCD", 4), Equals(true));

      NetMessageReader r(w.Data(), w.Size());
      char out[4] = {'W', 'X', 'Y', 'Z'};
      Assert::That(r.ReadString(out, 4), Equals(false));
      Assert::That(out[0], Equals('\0'));
    };
    It(should_accept_a_terminated_string_that_exactly_fills_the_buffer) {
      NetMessageWriter w;
      Assert::That(w.WriteString("abc"), Equals(true)); // 3 chars + NUL
      NetMessageReader r(w.Data(), w.Size());
      char out[4] = {};
      Assert::That(r.ReadString(out, 4), Equals(true));
      Assert::That(std::string(out), Equals("abc"));
    };
    It(should_reject_a_string_whose_length_runs_past_the_payload) {
      NetMessageWriter w;
      Assert::That(w.WriteInt(9), Equals(true)); // claims nine bytes
      Assert::That(w.WriteRaw("ab", 2), Equals(true));
      NetMessageReader r(w.Data(), w.Size());
      char out[16] = {};
      Assert::That(r.ReadString(out, sizeof(out)), Equals(false));
    };
    It(should_reject_non_positive_string_lengths) {
      NetMessageWriter zero;
      Assert::That(zero.WriteInt(0), Equals(true));
      NetMessageReader rz(zero.Data(), zero.Size());
      char out[8] = {};
      Assert::That(rz.ReadString(out, sizeof(out)), Equals(false));

      NetMessageWriter negative;
      Assert::That(negative.WriteInt(-3), Equals(true));
      NetMessageReader rn(negative.Data(), negative.Size());
      Assert::That(rn.ReadString(out, sizeof(out)), Equals(false));
    };
  };

  // The handshake nonce is the only thing standing between a spoofed source
  // address and a live connection, so it must not be recoverable from the
  // nonces a peer has already seen. Unpredictability itself is a property of
  // the construction (ChaCha20 keyed from the OS entropy source, see
  // NetNonce32) rather than something a unit spec can assert; what is pinned
  // here is the observable contract the old xorshift also had to meet.
  Describe(Nonces) {
    It(should_not_repeat_across_a_large_sample) {
      const int kSamples = 4096;
      std::set<uint32_t> seen;
      for (int i = 0; i < kSamples; i++)
        seen.insert(NetNonce32());
      Assert::That((int)seen.size(), Equals(kSamples));
    };
    It(should_set_every_bit_position_about_half_the_time) {
      const int kSamples = 4096;
      int ones[32] = {};
      for (int i = 0; i < kSamples; i++) {
        uint32_t n = NetNonce32();
        for (int b = 0; b < 32; b++)
          if (n & (1u << b))
            ones[b]++;
      }
      for (int b = 0; b < 32; b++) {
        Assert::That(ones[b], Is().GreaterThan(kSamples * 2 / 5));
        Assert::That(ones[b], Is().LessThan(kSamples * 3 / 5));
      }
    };
    It(should_never_return_a_constant) {
      uint32_t first = NetNonce32();
      bool differs = false;
      for (int i = 0; i < 32 && !differs; i++)
        differs = NetNonce32() != first;
      Assert::That(differs, Equals(true));
    };
  };
};
