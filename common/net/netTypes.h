#pragma once

#include <cstdint>

// ── Networking core types (SDL-free) ────────────────────────────────────────
// IPv4 UDP transport for hosting and joining games, built on the patterns from
// Teeworlds 0.7.5 (zlib license): reliability on demand, chunk batching, a
// cookie handshake, and tick-based snapshot deltas. Everything here is platform
// neutral so the layer can be spec'd without sockets.

// Wire limits. 1400 bytes keeps a datagram inside the Ethernet MTU so no
// IP fragmentation occurs.
constexpr int kNetMaxPacketSize = 1400;
constexpr int kNetMaxPayload = kNetMaxPacketSize - 7; // connected packet header
constexpr int kNetMaxChunksPerPacket = 256;
constexpr int kNetMaxChunkSize = 1200;
constexpr int kNetSequenceBits = 10;
constexpr int kNetMaxSequence = 1 << kNetSequenceBits; // sequence wraps at 1024

// Connection timing, milliseconds.
constexpr uint32_t kNetTimeoutMs = 10000;       // no traffic at all
constexpr uint32_t kNetResendMs = 1000;         // unacked vital chunk resend
constexpr uint32_t kNetHardResendMs = 10000;    // oldest unacked for too long
constexpr uint32_t kNetFlushMs = 500;           // auto-flush queued chunks
constexpr uint32_t kNetKeepaliveMs = 1000;      // idle keepalive
constexpr uint32_t kNetHandshakeRetryMs = 500;  // handshake step resend

// Connection limits.
constexpr int kNetMaxClients = 16;              // fixed server slots
constexpr int kNetMaxClientsPerIp = 4;          // anti-flood cap per address
constexpr int kNetResendBufferSize = 16 * 1024; // unacked vital byte pool
constexpr int kNetResendMaxEntries = 96;        // unacked vital chunk entries

enum NetChunkFlag { kNetChunkVital = 1, kNetChunkResend = 2 };

enum NetPacketFlag { kNetPacketResend = 1 };

// An IPv4 endpoint. Both fields are network byte order, ready for the socket
// layer without conversion.
struct NetAddress {
    uint32_t ip = 0;   // like sockaddr_in::sin_addr.s_addr
    uint16_t port = 0; // like sockaddr_in::sin_port
    bool operator==(const NetAddress &o) const { return ip == o.ip && port == o.port; }
    bool operator!=(const NetAddress &o) const { return !(*this == o); }
};

// One opaque message delivered by a connection. vital chunks arrive lossless
// and in order; non-vital chunks may be dropped. Data points into connection
// scratch memory and stays valid until the next Feed.
struct NetChunk {
    const uint8_t *data = nullptr;
    int size = 0;
    bool vital = false;
};
