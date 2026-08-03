#include <cstring>

#include "netPacket.h"

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
  int32_t len = 0;
  if (!ReadInt(len))
    return false;
  if (len < 1 || len > outSize)
    return false;
  if (!ReadRaw(out, len))
    return false;
  if (out[len - 1] != '\0') // enforce the terminator
    out[outSize - 1] = '\0';
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
