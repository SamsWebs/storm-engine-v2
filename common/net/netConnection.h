#pragma once

#include <cstdint>
#include <functional>

#include "netPacket.h"
#include "netTypes.h"

// ── Reliable connection over UDP (SDL-free) ─────────────────────────────────
// One connection talks to one peer. Chunks are batched into 1400-byte
// datagrams; vital chunks carry 10-bit sequence numbers, are retransmitted
// until acked, and are delivered in order. Non-vital chunks are never
// retransmitted and may arrive in any order. This is the Teeworlds
// CNetConnection design: a wrap-around ack/sequence window, a "backroom" test
// to drop duplicates, a ring buffer of unacked vital chunks, receiver-driven
// resend requests (the RESEND packet flag), and 1s sender-side retries.
//
// The connection owns no socket: the caller provides a send callback, feeds
// received datagrams, and pumps Update() with a millisecond clock (the same
// pattern as the pure input/ layer).

class NetConnection {
public:
  enum State { kOffline, kOnline, kError };

  using SendFunc = std::function<bool(const uint8_t *packet, int size)>;

  NetConnection() = default;

  void SetSendFunc(SendFunc fn) { send_ = fn; }
  // token = the nonce we issued, verified on incoming packets; peerToken =
  // the nonce the peer issued, stamped on outgoing packets. A peer that was
  // never handed a cookie cannot forge either direction.
  void Start(uint32_t nowMs, uint32_t token, uint32_t peerToken);
  void Stop();
  State GetState() const { return state_; }
  bool IsOnline() const { return state_ == kOnline; }
  const char *GetError() const { return error_; }

  bool QueueChunk(bool vital, const void *data, int size);
  void Flush(bool force = false);

  // Feeds one received datagram. Returns 1 if accepted (chunks may be
  // pending), 0 if benign (keepalive/duplicate), -1 if rejected (bad token,
  // malformed, offline). packet must be at most kNetMaxPacketSize bytes.
  // Chunk data from NextChunk stays valid until the next Feed.
  int Feed(const uint8_t *packet, int size);
  bool NextChunk(NetChunk &out);

  // Call every frame with the game clock. Drives retransmits, keepalives,
  // auto-flush, and timeouts.
  void Update(uint32_t nowMs);

  uint32_t GetRTT() const { return rtt_; }
  int GetAck() const { return ack_; }
  int GetSequence() const { return sequence_; }
  bool HasQueuedChunks() const { return constructChunks_ != 0; }

private:
  struct ResendEntry {
    int sequence;
    int size;
    int offset; // byte offset of this entry's data in resendData_
    uint32_t firstSendMs;
    uint32_t lastSendMs;
  };

  static bool IsSeqInBackroom(int seq, int ack);
  bool AckInRange(int ack);
  bool AppendToConstruct(int flags, int sequence, const void *data, int size);
  bool BufferVital(int sequence, const void *data, int size);
  void AckChunks(int ack, uint32_t nowMs);
  void Retransmit(ResendEntry &entry);
  void ResendAll();
  void SetError(const char *msg);

  uint8_t resendData_[kNetResendBufferSize];
  int resendWrite_ = 0;
  ResendEntry entries_[kNetResendMaxEntries];
  int entryHead_ = 0;
  int entryTail_ = 0;
  int entryCount_ = 0;

  uint8_t construct_[kNetMaxPayload];
  int constructSize_ = 0;
  int constructChunks_ = 0;
  bool resendRequested_ = false;

  uint8_t pending_[kNetMaxPayload];
  int pendingOffsets_[kNetMaxChunksPerPacket];
  int pendingSizes_[kNetMaxChunksPerPacket];
  bool pendingVital_[kNetMaxChunksPerPacket];
  int pendingCount_ = 0;
  int pendingRead_ = 0;
  int pendingPos_ = 0;

  State state_ = kOffline;
  uint32_t token_ = 0;
  uint32_t peerToken_ = 0;
  int sequence_ = 0;
  int ack_ = 0;
  uint32_t lastSendMs_ = 0;
  uint32_t lastRecvMs_ = 0;
  uint32_t nowMs_ = 0;
  uint32_t rtt_ = 0;
  char error_[64] = {};
  SendFunc send_;
};
