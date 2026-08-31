#include <cstring>

#include "netConnection.h"

namespace storm {

void NetConnection::Start(uint32_t nowMs, uint32_t token, uint32_t peerToken) {
  state_ = kOnline;
  token_ = token;
  peerToken_ = peerToken;
  sequence_ = 0;
  ack_ = 0;
  nowMs_ = nowMs;
  lastSendMs_ = nowMs;
  lastRecvMs_ = nowMs;
  rtt_ = 0;
  error_[0] = '\0';
  constructSize_ = 0;
  constructChunks_ = 0;
  resendRequested_ = false;
  resendWrite_ = 0;
  entryHead_ = 0;
  entryTail_ = 0;
  entryCount_ = 0;
  pendingCount_ = 0;
  pendingRead_ = 0;
}

void NetConnection::Stop() {
  state_ = kOffline;
  constructSize_ = 0;
  constructChunks_ = 0;
  resendRequested_ = false;
  entryCount_ = 0;
  pendingCount_ = 0;
  pendingRead_ = 0;
}

void NetConnection::SetError(const char *msg) {
  state_ = kError;
  std::strncpy(error_, msg, sizeof(error_) - 1);
  error_[sizeof(error_) - 1] = '\0';
}

bool NetConnection::IsSeqInBackroom(int seq, int ack) {
  int bottom = ack - kNetMaxSequence / 2;
  if (bottom < 0) {
    if (seq <= ack)
      return true;
    if (seq >= bottom + kNetMaxSequence)
      return true;
  } else {
    if (seq <= ack && seq >= bottom)
      return true;
  }
  return false;
}

bool NetConnection::AckInRange(int ack) {
  // The peer may only ack sequences we recently handed out: at most half
  // the sequence space behind the newest sequence. Computed with 10-bit
  // wraparound so acks stay valid across the 1023 -> 0 rollover.
  int mask = kNetMaxSequence - 1;
  if (((sequence_ - ack) & mask) >= kNetMaxSequence / 2)
    return false;
  return true;
}

bool NetConnection::BufferVital(int sequence, const void *data, int size) {
  if (entryCount_ >= kNetResendMaxEntries)
    return false;
  int offset = resendWrite_;
  if (resendWrite_ + size > kNetResendBufferSize) {
    // The write index crosses the end of the buffer, so this chunk lands
    // at [0, overflowLen) after wrapping. That is safe only if those
    // bytes are not pinned by a live (unacked) entry — wrapping into
    // live bytes makes the next retransmit read back the wrong data
    // (P2). Ring arithmetic alone cannot prove it: entries are freed in
    // order while wrapped entries sit below the read frontier, so the
    // only exact check is a scan of the live entries.
    offset = 0;
    resendWrite_ = 0;
  }
  for (int i = 0; i < entryCount_; i++) {
    const ResendEntry &e = entries_[(entryHead_ + i) % kNetResendMaxEntries];
    int d1 =
        (e.offset - resendWrite_ + kNetResendBufferSize) % kNetResendBufferSize;
    int d2 =
        (resendWrite_ - e.offset + kNetResendBufferSize) % kNetResendBufferSize;
    if (d1 < size || d2 < e.size)
      return false; // new chunk overlaps a live entry
  }
  std::memcpy(resendData_ + resendWrite_, data, size);
  ResendEntry &e = entries_[entryTail_];
  e.sequence = sequence;
  e.size = size;
  e.offset = offset;
  e.firstSendMs = nowMs_;
  e.lastSendMs = nowMs_;
  entryTail_ = (entryTail_ + 1) % kNetResendMaxEntries;
  entryCount_++;
  resendWrite_ += size;
  return true;
}

bool NetConnection::AppendToConstruct(int flags, int sequence, const void *data,
                                      int size) {
  int headerSize = NetChunkHeader::PackedSize(flags);
  if (constructSize_ + headerSize + size > kNetMaxPayload ||
      constructChunks_ == kNetMaxChunksPerPacket)
    Flush(false);
  if (constructSize_ + headerSize + size > kNetMaxPayload)
    return false;

  NetChunkHeader header;
  header.size = size;
  header.sequence = sequence;
  header.flags = flags;
  uint8_t *dst = construct_ + constructSize_;
  if (!header.Pack(dst))
    return false;
  if (size > 0)
    std::memcpy(dst + headerSize, data, size);
  constructSize_ += headerSize + size;
  constructChunks_++;
  return true;
}

bool NetConnection::QueueChunk(bool vital, const void *data, int size) {
  if (state_ != kOnline || size < 0 || size > kNetMaxChunkSize)
    return false;

  int sequence = sequence_;
  if (vital) {
    sequence = (sequence_ + 1) & (kNetMaxSequence - 1);
    sequence_ = sequence;
    if (!BufferVital(sequence, data, size)) {
      SetError("too weak connection (out of buffer)");
      return false;
    }
    return AppendToConstruct(kNetChunkVital, sequence, data, size);
  }
  return AppendToConstruct(0, 0, data, size);
}

void NetConnection::Flush(bool force) {
  if (state_ != kOnline) {
    constructSize_ = 0;
    constructChunks_ = 0;
    resendRequested_ = false;
    return;
  }
  if (!force && constructChunks_ == 0 && !resendRequested_)
    return;

  uint8_t packet[kNetMaxPacketSize];
  NetPacketHeaderPack(packet, resendRequested_ ? kNetPacketResend : 0, ack_,
                      constructChunks_, peerToken_);
  if (constructSize_ > 0)
    std::memcpy(packet + 7, construct_, constructSize_);
  if (!send_ || !send_(packet, 7 + constructSize_)) {
    SetError("send failed");
    return;
  }
  lastSendMs_ = nowMs_;
  constructSize_ = 0;
  constructChunks_ = 0;
  resendRequested_ = false;
}

void NetConnection::AckChunks(int ack, uint32_t nowMs) {
  while (entryCount_ > 0) {
    ResendEntry &e = entries_[entryHead_];
    if (!IsSeqInBackroom(e.sequence, ack))
      break;
    uint32_t sample = nowMs - e.firstSendMs;
    rtt_ = rtt_ ? (rtt_ * 7 + sample) / 8 : sample;
    entryHead_ = (entryHead_ + 1) % kNetResendMaxEntries;
    entryCount_--;
  }
}

void NetConnection::Retransmit(ResendEntry &entry) {
  if (AppendToConstruct(kNetChunkVital | kNetChunkResend, entry.sequence,
                        resendData_ + entry.offset, entry.size))
    entry.lastSendMs = nowMs_;
}

void NetConnection::ResendAll() {
  for (int i = 0; i < entryCount_; i++)
    Retransmit(entries_[(entryHead_ + i) % kNetResendMaxEntries]);
}

int NetConnection::Feed(const uint8_t *packet, int size) {
  if (state_ != kOnline || size < 0 || size > kNetMaxPacketSize)
    return -1;

  int flags = 0, ack = 0, numChunks = 0;
  uint32_t token = 0;
  if (!NetPacketHeaderUnpack(packet, size, flags, ack, numChunks, token))
    return -1;
  if (token != token_)
    return -1;
  if (!AckInRange(ack))
    return -1;

  if (flags & kNetPacketResend)
    ResendAll();
  AckChunks(ack, nowMs_);
  lastRecvMs_ = nowMs_;
  if (numChunks == 0)
    return 0; // keepalive

  pendingCount_ = 0;
  pendingRead_ = 0;
  pendingPos_ = 0;
  int pos = 7;
  for (int i = 0; i < numChunks; i++) {
    if (pos >= size)
      return -1;
    NetChunkHeader header;
    int consumed = 0;
    if (!header.Unpack(packet + pos, size - pos, consumed))
      return -1;
    pos += consumed;
    if (header.size < 0 || pos + header.size > size)
      return -1;

    bool deliver = true;
    if (header.flags & kNetChunkVital) {
      int expected = (ack_ + 1) & (kNetMaxSequence - 1);
      if (header.sequence == expected) {
        ack_ = header.sequence;
      } else if (IsSeqInBackroom(header.sequence, ack_)) {
        deliver = false; // duplicate
      } else {
        resendRequested_ = true; // gap: ask the peer to resend
        deliver = false;
      }
    }

    if (deliver) {
      if (pendingCount_ >= kNetMaxChunksPerPacket)
        return -1;
      if (header.size > 0)
        std::memcpy(pending_ + pendingPos_, packet + pos, header.size);
      pendingOffsets_[pendingCount_] = pendingPos_;
      pendingSizes_[pendingCount_] = header.size;
      pendingVital_[pendingCount_] = (header.flags & kNetChunkVital) != 0;
      pendingPos_ += header.size;
      pendingCount_++;
    }
    pos += header.size;
  }
  return 1;
}

bool NetConnection::NextChunk(NetChunk &out) {
  if (pendingRead_ >= pendingCount_)
    return false;
  int i = pendingRead_++;
  out.data = pending_ + pendingOffsets_[i];
  out.size = pendingSizes_[i];
  out.vital = pendingVital_[i];
  return true;
}

void NetConnection::Update(uint32_t nowMs) {
  nowMs_ = nowMs;
  if (state_ != kOnline)
    return;

  if (entryCount_ > 0) {
    ResendEntry &e = entries_[entryHead_];
    if (nowMs - e.firstSendMs > kNetHardResendMs) {
      SetError("too weak connection");
      return;
    }
    if (nowMs - e.lastSendMs > kNetResendMs)
      Retransmit(e);
  }

  if (nowMs - lastSendMs_ > kNetFlushMs)
    Flush(false);
  if (nowMs - lastRecvMs_ > kNetTimeoutMs) {
    SetError("timeout");
    return;
  }
  if (nowMs - lastSendMs_ > kNetKeepaliveMs)
    Flush(true); // idle keepalive
}

} // namespace storm
