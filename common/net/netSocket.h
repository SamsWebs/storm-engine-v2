#pragma once

#include <string>

#include "../logger.h"
#include "netTypes.h"

namespace storm {

// ── Platform UDP socket ─────────────────────────────────────────────────────
// Thin wrapper over BSD sockets: Linux, macOS, Android (NDK) and Switch (libnx
// provides the same API); Windows via winsock behind #ifdef. Non-blocking —
// Recv returns -1 when nothing is pending. The socket layer is the only part
// of the net module that touches the OS; everything else is pure logic.

class NetSocket {
public:
  NetSocket() = default;
  ~NetSocket();

  // Owns a socket descriptor: a copy would give two objects and two
  // destructors closing one descriptor. KNOWN_ISSUES item 6, fixed in 2.0.0.
  NetSocket(const NetSocket &) = delete;
  NetSocket &operator=(const NetSocket &) = delete;

  bool Open(uint16_t port); // 0 = OS-assigned ephemeral port
  void Close();
  bool IsOpen() const { return fd_ != -1; }
  bool Send(const NetAddress &addr, const uint8_t *data, int size);
  int Recv(NetAddress &addr, uint8_t *data, int maxSize); // -1 = none/error
  uint16_t GetBoundPort() const;                          // host byte order

private:
  int fd_ = -1;
  Logger logger_;
};

// ── Why an Open failed ──────────────────────────────────────────────────────
// Added 2.7.2, with the Switch platform-init arm. Open() has three failure
// stages and used to report all of them with one line -- "failed to create
// non-blocking UDP socket" -- which is a true statement about the middle one
// and a false one about the other two. The cost was paid on Switch: libnx
// routes BSD sockets through the bsd: service, which is inert until
// socketInitializeDefault() has run, so a missing platform init surfaced as a
// socket-creation complaint. A developer reading that goes looking at a
// firewall, which is three wrong guesses away from the actual cause.
//
// Three stages, three causes, three fixes, three lines. Spec'd in
// specs/net/netSocket.spec.cpp.
enum class NetSocketOpenFailure {
  None,         // no failure
  PlatformInit, // the OS networking stack refused to start (WSAStartup, or
                // socketInitializeDefault on Switch)
  Socket,       // socket() or the non-blocking switch failed
  Bind,         // the port was not available -- the one a player can act on
};

// The decision inside Open() between the "opened" and "made non-blocking"
// outcomes, with the platform arms left to fill in the two booleans. Split out
// so the choice is one spec'd decision rather than an #ifdef straddling the
// middle of a condition. The socketOk==false case ignores nonBlockingOk on
// purpose: once socket() has failed there is nothing to make non-blocking, and
// the earliest thing that broke is the one worth naming.
NetSocketOpenFailure NetClassifySocketSetup(bool socketOk, bool nonBlockingOk);

// The line Open() logs for a stage. Empty for None -- a success is not a log
// line. Every non-empty message starts with "NetSocket: ", so the three share
// a prefix and one grep still finds every socket line. Unknown enumerators
// produce a generic message rather than nothing, because the failure mode
// being fixed here is silence.
std::string NetSocketOpenFailureMessage(NetSocketOpenFailure failure,
                                        uint16_t port);

// ── libnx Result decoding ───────────────────────────────────────────────────
// Switch reports failures as a 32-bit Result, not an errno, and "socket init
// failed" without the code is four guesses and a reboot. The layout is
// libnx's (switch/result.h): module in the low 9 bits, description from bit 9.
// These are not round numbers -- 0x1FF and >>9, not 0x3FF and >>10 -- so they
// are spec'd here rather than trusted, and the Switch build static_asserts
// them against R_MODULE/R_DESCRIPTION in the same TU.
//
// Portable and harmless off Switch: nothing but a bit layout, no libnx type
// in the signature. constexpr so the Switch build can static_assert the layout
// against libnx's own macros (see netSocket.cpp) -- a static_assert cannot call
// a function whose definition it cannot see, and it cannot see this one from
// another TU.
inline constexpr uint32_t NetResultModule(uint32_t result) {
  return result & 0x1FFu;
}

inline constexpr uint32_t NetResultDescription(uint32_t result) {
  return (result >> 9) & 0x1FFFu;
}

// Human-readable detail for a FAILED Result, composed onto the platform-init
// line. Empty for 0, which is libnx success (R_SUCCEEDED is == 0) and never
// reaches a failure line.
std::string NetResultText(uint32_t result);

// Address helpers. NetAddress stores network-byte-order fields; these bridge
// to host order and strings.
NetAddress NetAddressFromParts(uint32_t ipHost, uint16_t portHost);
uint32_t NetIpToHost(const NetAddress &addr);
uint16_t NetPortToHost(const NetAddress &addr);
std::string NetAddressToString(const NetAddress &addr);
// Resolves a hostname or dotted IPv4 (getaddrinfo, first IPv4 result).
bool NetResolveAddress(const std::string &hostOrIp, uint16_t port,
                       NetAddress &out);

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

} // namespace storm
