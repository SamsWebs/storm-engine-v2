#pragma once

#include <cstdint>

#include "netTypes.h"
#include "netVarInt.h"

namespace storm {

// ── Packet framing and message packing (SDL-free) ───────────────────────────
// Connected packet layout (7-byte header, then chunk data):
//   byte0: flags(6 bits, NetPacketFlag) << 2 | ackHigh(2 bits)
//   byte1: ackLow(8 bits)
//   byte2: numChunks
//   byte3-6: token, network byte order (the nonce the peer issued us)
//
// Chunk header (2 bytes plain, 3 bytes vital):
//   byte0: flags(2 bits, NetChunkFlag) << 6 | sizeHigh(6 bits)
//   byte1: sizeLow(6 bits) | seqHigh(2 bits) << 6   (vital only)
//   byte2: seqLow(8 bits)                            (vital only)
// Sizes are 12 bits; sequences are 10 bits.

// ── Chunk header ──

struct NetChunkHeader {
  int size = 0;
  int sequence = 0;
  int flags = 0; // NetChunkFlag

  bool Pack(uint8_t *dst) const;                               // 2 or 3 bytes
  bool Unpack(const uint8_t *src, int srcSize, int &consumed); // bounded
  static int PackedSize(int flags) { return (flags & kNetChunkVital) ? 3 : 2; }
};

// ── Connected packet header ──

void NetPacketHeaderPack(uint8_t *dst, int flags, int ack, int numChunks,
                         uint32_t token);
// Fails on truncated packets. token is a uint32 in network byte order.
bool NetPacketHeaderUnpack(const uint8_t *src, int srcSize, int &flags,
                           int &ack, int &numChunks, uint32_t &token);

// ── Control datagrams (pre-connection) ──
// First byte 0xCF marks a control datagram. A connected packet can never start
// with it: flags << 2 | ackHigh is at most 0x07. Control traffic is how the
// cookie handshake works before any connection exists, so every message is
// re-sent by the sender until the protocol advances (see netServer/netClient).
//   CONNECT:        client -> server [clientNonce 4B]
//   CONNECT_ACCEPT: server -> client [serverNonce 4B]
//   CONNECT_READY:  client -> server [clientNonce 4B][serverNonce 4B]
//   ACCEPT:         server -> client [serverNonce 4B]
//   CLOSE:          either          [reason string]

constexpr uint8_t kNetControlMagic = 0xCF;

enum NetControlMessage {
  kNetControlConnect = 1,
  kNetControlConnectAccept = 2,
  kNetControlConnectReady = 3,
  kNetControlAccept = 4,
  kNetControlClose = 5,
};

struct NetControlPacket {
  int message = 0;
  uint8_t payload[kNetMaxPayload] = {};
  int payloadSize = 0;

  bool Pack(uint8_t *dst, int dstSize, int &outSize) const;
  static bool IsControl(const uint8_t *data, int size);
  bool Unpack(const uint8_t *data, int size);
};

class NetSocket; // netSocket.h — only a reference is needed here

// Packs one control datagram and sends it. payloadSize is clamped to
// kNetMaxPayload so an over-long reason string can never overflow the frame.
// Both NetServer::SendControl and NetClient::SendControl forward here so the
// bound lives in exactly one place. Returns false if nothing was sent.
bool NetSendControl(NetSocket &sock, const NetAddress &to, int message,
                    const void *payload, int payloadSize);

// ── Handshake nonces ──
// The cookie handshake's whole guarantee is that a joining peer cannot guess
// the nonce it has to echo back, and the token derived from it is the only
// check on an inbound connected packet. NetRandom32 (netSocket.h) is a raw
// xorshift64: every output bit is a linear function of the state, so a handful
// of observed nonces recover it and every future nonce with it. This draws
// from a ChaCha20 keystream keyed once from std::random_device (the OS entropy
// source on every platform the engine targets, mixed with the clock and the
// process's own layout), so the output is not invertible even if the seed is
// weaker than advertised. Use it for anything a peer gets to see.
uint32_t NetNonce32();

// ── Message packing ──
// A tiny varint-based writer/reader for game-defined messages. Games give each
// message type a number, write it first, then its fields.

class NetMessageWriter {
public:
  static constexpr int kMaxSize = kNetMaxChunkSize;

  bool WriteInt(int32_t value);
  bool WriteString(const char *str); // length + bytes incl. terminator
  bool WriteRaw(const void *data, int size);
  int Size() const { return size_; }
  const uint8_t *Data() const { return data_; }
  void Reset() { size_ = 0; }

private:
  uint8_t data_[kMaxSize];
  int size_ = 0;
};

class NetMessageReader {
public:
  NetMessageReader(const uint8_t *data, int size) : data_(data), size_(size) {}

  bool ReadInt(int32_t &value);
  // Always null-terminates out. Fails (leaving out empty) if the wire string
  // does not carry its own terminator, so a truncated or hostile packet can
  // never make the caller read past what the sender actually sent.
  bool ReadString(char *out, int outSize);
  bool ReadRaw(void *out, int size);
  bool Finished() const { return pos_ == size_; }
  int Position() const { return pos_; }

private:
  const uint8_t *data_;
  int size_;
  int pos_ = 0;
};

} // namespace storm
