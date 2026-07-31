#pragma once

#include <string>

#include "../logger.h"
#include "netTypes.h"

// ── Platform UDP socket ─────────────────────────────────────────────────────
// Thin wrapper over BSD sockets: Linux, macOS, Android (NDK) and Switch (libnx
// provides the same API); Windows via winsock behind #ifdef. Non-blocking —
// Recv returns -1 when nothing is pending. The socket layer is the only part
// of the net module that touches the OS; everything else is pure logic.

class NetSocket {
public:
    NetSocket() = default;
    ~NetSocket();

    bool Open(uint16_t port); // 0 = OS-assigned ephemeral port
    void Close();
    bool IsOpen() const { return fd_ != -1; }
    bool Send(const NetAddress &addr, const uint8_t *data, int size);
    int Recv(NetAddress &addr, uint8_t *data, int maxSize); // -1 = none/error
    uint16_t GetBoundPort() const; // host byte order

private:
    int fd_ = -1;
    Logger logger_;
};

// Address helpers. NetAddress stores network-byte-order fields; these bridge
// to host order and strings.
NetAddress NetAddressFromParts(uint32_t ipHost, uint16_t portHost);
uint32_t NetIpToHost(const NetAddress &addr);
uint16_t NetPortToHost(const NetAddress &addr);
std::string NetAddressToString(const NetAddress &addr);
// Resolves a hostname or dotted IPv4 (getaddrinfo, first IPv4 result).
bool NetResolveAddress(const std::string &hostOrIp, uint16_t port, NetAddress &out);

// Millisecond clock (steady) and a nonce source for handshakes. Both are
// platform agnostic but live here with the OS-touching code.
uint32_t NetNowMs();
uint32_t NetRandom32();

// Nonces travel as raw 4-byte payloads and double as packet-header tokens.
inline uint32_t NonceToToken(const uint8_t nonce[4]) {
    return ((uint32_t)nonce[0] << 24) | ((uint32_t)nonce[1] << 16) |
           ((uint32_t)nonce[2] << 8) | (uint32_t)nonce[3];
}

inline void TokenToNonce(uint32_t token, uint8_t nonce[4]) {
    nonce[0] = (uint8_t)(token >> 24);
    nonce[1] = (uint8_t)(token >> 16);
    nonce[2] = (uint8_t)(token >> 8);
    nonce[3] = (uint8_t)token;
}
