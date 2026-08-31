#include <cstring>
#include <random>

#include "netPacket.h"
#include "netSocket.h"

namespace storm {

bool NetChunkHeader::Pack(uint8_t *dst) const {
  if (size < 0 || size > 0x0FFF)
    return false;
  if (flags & ~0x03)
    return false;

  dst[0] = (uint8_t)((flags & 0x03) << 6) | (uint8_t)((size >> 6) & 0x3F);
  dst[1] = (uint8_t)(size & 0x3F);
  if (flags & kNetChunkVital) {
    dst[1] |= (uint8_t)((sequence >> 2) & 0xC0);
    dst[2] = (uint8_t)(sequence & 0xFF);
  }
  return true;
}

bool NetChunkHeader::Unpack(const uint8_t *src, int srcSize, int &consumed) {
  if (srcSize < 2)
    return false;

  flags = (src[0] >> 6) & 0x03;
  size = ((src[0] & 0x3F) << 6) | (src[1] & 0x3F);
  sequence = 0;
  if (flags & kNetChunkVital) {
    if (srcSize < 3)
      return false;
    sequence = ((src[1] & 0xC0) << 2) | src[2];
    consumed = 3;
  } else {
    consumed = 2;
  }
  return true;
}

void NetPacketHeaderPack(uint8_t *dst, int flags, int ack, int numChunks,
                         uint32_t token) {
  dst[0] = (uint8_t)((flags & 0x3F) << 2) | (uint8_t)((ack >> 8) & 0x03);
  dst[1] = (uint8_t)(ack & 0xFF);
  dst[2] = (uint8_t)(numChunks & 0xFF);
  dst[3] = (uint8_t)((token >> 24) & 0xFF);
  dst[4] = (uint8_t)((token >> 16) & 0xFF);
  dst[5] = (uint8_t)((token >> 8) & 0xFF);
  dst[6] = (uint8_t)(token & 0xFF);
}

bool NetPacketHeaderUnpack(const uint8_t *src, int srcSize, int &flags,
                           int &ack, int &numChunks, uint32_t &token) {
  if (srcSize < 7)
    return false;
  flags = (src[0] >> 2) & 0x3F;
  ack = ((src[0] & 0x03) << 8) | src[1];
  numChunks = src[2];
  token = ((uint32_t)src[3] << 24) | ((uint32_t)src[4] << 16) |
          ((uint32_t)src[5] << 8) | (uint32_t)src[6];
  return true;
}

bool NetControlPacket::Pack(uint8_t *dst, int dstSize, int &outSize) const {
  int total = 2 + payloadSize;
  if (dstSize < total)
    return false;
  dst[0] = kNetControlMagic;
  dst[1] = (uint8_t)message;
  if (payloadSize > 0)
    std::memcpy(dst + 2, payload, payloadSize);
  outSize = total;
  return true;
}

bool NetControlPacket::IsControl(const uint8_t *data, int size) {
  return size >= 2 && data[0] == kNetControlMagic;
}

bool NetControlPacket::Unpack(const uint8_t *data, int size) {
  if (size < 2 || data[0] != kNetControlMagic)
    return false;
  if (size - 2 > kNetMaxPayload)
    return false;
  message = data[1];
  std::memcpy(payload, data + 2, size - 2);
  payloadSize = size - 2;
  return true;
}

bool NetSendControl(NetSocket &sock, const NetAddress &to, int message,
                    const void *payload, int payloadSize) {
  if (payloadSize < 0)
    return false;
  if (payloadSize > kNetMaxPayload)
    payloadSize = kNetMaxPayload; // truncate: never overflow the frame
  NetControlPacket ctrl;
  ctrl.message = message;
  if (payload && payloadSize > 0)
    std::memcpy(ctrl.payload, payload, payloadSize);
  ctrl.payloadSize = payloadSize;
  uint8_t buf[kNetMaxPacketSize];
  int size = 0;
  if (!ctrl.Pack(buf, sizeof(buf), size))
    return false;
  return sock.Send(to, buf, size);
}

namespace {

inline uint32_t Rotl32(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

inline void ChaChaQuarter(uint32_t &a, uint32_t &b, uint32_t &c, uint32_t &d) {
  a += b;
  d = Rotl32(d ^ a, 16);
  c += d;
  b = Rotl32(b ^ c, 12);
  a += b;
  d = Rotl32(d ^ a, 8);
  c += d;
  b = Rotl32(b ^ c, 7);
}

// ChaCha20 block function: ten double rounds over a 16-word state, added back
// to the input. Nothing here is reversible from the output, which is the whole
// point — see NetNonce32 in netPacket.h.
void ChaChaBlock(const uint32_t in[16], uint32_t out[16]) {
  for (int i = 0; i < 16; i++)
    out[i] = in[i];
  for (int round = 0; round < 10; round++) {
    ChaChaQuarter(out[0], out[4], out[8], out[12]);
    ChaChaQuarter(out[1], out[5], out[9], out[13]);
    ChaChaQuarter(out[2], out[6], out[10], out[14]);
    ChaChaQuarter(out[3], out[7], out[11], out[15]);
    ChaChaQuarter(out[0], out[5], out[10], out[15]);
    ChaChaQuarter(out[1], out[6], out[11], out[12]);
    ChaChaQuarter(out[2], out[7], out[8], out[13]);
    ChaChaQuarter(out[3], out[4], out[9], out[14]);
  }
  for (int i = 0; i < 16; i++)
    out[i] += in[i];
}

// One keystream block at a time, 16 words each. A handshake draws one word per
// join, and the 64-bit block counter is good for 2^64 blocks, so the stream is
// never exhausted and never rekeyed.
struct NonceStream {
  uint32_t state[16] = {};
  uint32_t block[16] = {};
  int next = 16; // >= 16 forces a fresh block on first use

  NonceStream() {
    state[0] = 0x61707865; // "expand 32-byte k"
    state[1] = 0x3320646e;
    state[2] = 0x79622d32;
    state[3] = 0x6b206574;
    // 256-bit key. std::random_device is the OS CSPRNG on Linux and Windows;
    // NetRandom32 is xor'd in for its /dev/urandom read, so the key is at
    // least as strong as the better of the two sources.
    std::random_device rd;
    for (int i = 4; i < 12; i++)
      state[i] = (uint32_t)rd() ^ NetRandom32();
    state[12] = 0; // 64-bit block counter
    state[13] = 0;
    // Stream position: distinct per process even if every key word collided.
    state[14] = NetNowMs();
    state[15] = (uint32_t)(uintptr_t)this;
  }

  uint32_t Next() {
    if (next >= 16) {
      ChaChaBlock(state, block);
      if (++state[12] == 0)
        ++state[13];
      next = 0;
    }
    return block[next++];
  }
};

} // namespace

uint32_t NetNonce32() {
  static NonceStream stream;
  return stream.Next();
}

bool NetMessageWriter::WriteInt(int32_t value) {
  int n = NetVarIntPack(data_ + size_, kMaxSize - size_, value);
  if (n == 0)
    return false;
  size_ += n;
  return true;
}

bool NetMessageWriter::WriteString(const char *str) {
  int len = (int)std::strlen(str) + 1; // include terminator
  if (!WriteInt(len))
    return false;
  return WriteRaw(str, len);
}

bool NetMessageWriter::WriteRaw(const void *data, int size) {
  if (size < 0 || size_ + size > kMaxSize)
    return false;
  if (size > 0)
    std::memcpy(data_ + size_, data, size);
  size_ += size;
  return true;
}

bool NetMessageReader::ReadInt(int32_t &value) {
  int consumed = 0;
  if (!NetVarIntUnpack(data_ + pos_, size_ - pos_, value, consumed))
    return false;
  pos_ += consumed;
  return true;
}

bool NetMessageReader::ReadString(char *out, int outSize) {
  // Terminate up front so the documented postcondition holds on EVERY exit,
  // including the early failures below. docs/networking.md shows the return
  // value being discarded, so a caller that ignores it must still never read
  // uninitialised stack as if the peer had sent it.
  if (outSize > 0)
    out[0] = '\0';
  int32_t len = 0;
  if (!ReadInt(len))
    return false;
  if (len < 1 || len > outSize)
    return false;
  if (!ReadRaw(out, len))
    return false;
  if (out[len - 1] != '\0') {
    // Unterminated on the wire. Terminating at outSize - 1 instead would hand
    // the caller everything left in its own buffer past len as if the peer had
    // sent it, so refuse the read outright and leave out empty.
    out[0] = '\0';
    return false;
  }
  return true;
}

bool NetMessageReader::ReadRaw(void *out, int size) {
  if (size < 0 || pos_ + size > size_)
    return false;
  if (size > 0)
    std::memcpy(out, data_ + pos_, size);
  pos_ += size;
  return true;
}

} // namespace storm
