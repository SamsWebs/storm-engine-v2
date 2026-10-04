// inet_ntop requires _WIN32_WINNT >= 0x0600 (Vista), so this must be defined
// before the <winsock2.h> include further down this file. Nothing above that
// include needs it: netSocket.h pulls in only <string>, logger.h and
// netTypes.h, none of which reach winsock2.h or windows.h. It is a fallback
// for a compile that forgets the flag — every Windows build file here already
// passes -D_WIN32_WINNT=0x0600 (Makefile.win, examples/examples.win.mk), and
// the #ifndef below keeps this from colliding with them.
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#endif

// libnx first: its switch.h redefines parts of the newlib headers this file
// includes below, so it has to come before them. P7/2.7.2.
#ifdef __SWITCH__
#include <switch.h>
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>

#include "netSocket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
// mswsock.h for SIO_UDP_CONNRESET; it needs winsock2.h ahead of it.
#include <mswsock.h>
#include <process.h>
using SocketHandle = int;
#define NET_INVALID_SOCKET INVALID_SOCKET
#define NET_GETPID _getpid
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
#define NET_INVALID_SOCKET (-1)
#define NET_GETPID getpid
#endif

// POSIX guarantees INET_ADDRSTRLEN, but devkitA64's headers do not define it,
// so NetAddressToString did not compile for Switch at all -- a latent break
// that sat behind P6's missing common/net/ sources for as long as nobody ran
// the Switch build. 16 is the length POSIX fixes for IPv4 (46 would be
// overkill and hides a real overflow if inet_ntop ever changed family).
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

namespace storm {

// Windows refuses every entry point with WSANOTINITIALISED until WSAStartup
// has succeeded once in the process — name resolution included, not just
// socket(). Switch is the same shape with a different cause: libnx routes BSD
// sockets through the bsd: service, which is inert until
// socketInitializeDefault() has run, so socket(AF_INET, SOCK_DGRAM, 0) returns
// -1 on a Switch that has never started it. So every function here that touches
// the network calls this first, and it has to be idempotent: the function-local
// static runs its initializer exactly once, on whichever call gets there first,
// and is thread-safe by construction.
//
// No matching socketExit(), deliberately. The latch is process-wide and has
// three callers with no shared teardown point, and neither arm releases its
// handle today — WSAStartup has no matching WSACleanup here either. Adding a
// Switch-only shutdown would make the two platforms behave differently about
// something neither of them gets right yet; that is a 3.0 decision, not one to
// smuggle in with a portability fix.
static bool NetSocketsInit() {
#ifdef _WIN32
  static const bool ok = [] {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
  }();
  return ok;
#elif defined(__SWITCH__)
  static const bool ok = [] {
    return R_SUCCEEDED(socketInitializeDefault());
  }();
  return ok;
#else
  return true;
#endif
}

#ifdef __SWITCH__
// The libnx Result behind a failed init, so the log line can carry the code.
// Zero on Windows and POSIX: those platforms report WSAGetLastError() and
// errno at the point of failure instead, and a single latched code from a
// different call site would be a lie.
static uint32_t NetSocketsInitResult() {
  // Rerun the latch's own work rather than duplicating it: a second
  // socketInitializeDefault() would take a second bsd: session, and the
  // function-local static is the only thing allowed to call it.
  NetSocketsInit();
  return socketGetLastResult();
}
#endif

// What a failed platform init adds to its message. Platform-specific, and the
// call site stays platform-blind: only libnx has a structured error code worth
// printing, and Windows/POSIX have nothing extra to say here.
static std::string NetPlatformInitDetail() {
#ifdef __SWITCH__
  return NetResultText(NetSocketsInitResult());
#else
  return std::string();
#endif
}

static bool NetWouldBlock() {
#ifdef _WIN32
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EWOULDBLOCK || errno == EAGAIN;
#endif
}

// ── The pure half ───────────────────────────────────────────────────────────
// The part of Open() that decides, as opposed to the part that calls the OS.
// Split out so it can be spec'd on a machine whose socket() never fails, and
// so the three log lines have one owner.

NetSocketOpenFailure NetClassifySocketSetup(bool socketOk, bool nonBlockingOk) {
  if (!socketOk || !nonBlockingOk)
    return NetSocketOpenFailure::Socket;
  return NetSocketOpenFailure::None;
}

std::string NetSocketOpenFailureMessage(NetSocketOpenFailure failure,
                                        uint16_t port) {
  switch (failure) {
  case NetSocketOpenFailure::None:
    return std::string();
  case NetSocketOpenFailure::PlatformInit:
    // Deliberately says nothing about sockets. This is the line a Switch
    // developer sees when libnx was never started, and the whole reason the
    // three messages are separate is that the old shared line sent them
    // looking for a firewall.
    return "NetSocket: networking stack init failed (WSAStartup on Windows, "
           "socketInitializeDefault on Switch) before any socket call";
  case NetSocketOpenFailure::Socket:
    return "NetSocket: failed to create non-blocking UDP socket";
  case NetSocketOpenFailure::Bind:
    // Port 0 means "let the OS pick one", so "failed to bind UDP port 0" is a
    // contradiction the reader has to stop and think about.
    if (port == 0)
      return "NetSocket: failed to bind an OS-assigned UDP port";
    return "NetSocket: failed to bind UDP port " + std::to_string(port);
  }
  // A stage this build does not know about. Logging nothing here would put
  // back the one failure mode this file exists to remove.
  return "NetSocket: socket setup failed for an unrecognised reason";
}

// NetResultModule and NetResultDescription are inline constexpr in the header,
// so the static_asserts below can constant-evaluate them.
std::string NetResultText(uint32_t result) {
  // 0, spelled out rather than via libnx's R_SUCCEEDED: this function is
  // portable, and R_SUCCEEDED is a macro that only exists under __SWITCH__.
  // The static_asserts below hold the two definitions of "success" together.
  if (result == 0)
    return std::string();
  // Hex, because "0x" in front of a decimal number is a number you cannot look
  // up. %s keeps the std::to_string conversions out of the format string.
  char code[16];
  std::snprintf(code, sizeof(code), "0x%04X", result);
  return "libnx result " + std::string(code) + " (module " +
         std::to_string(NetResultModule(result)) + ", description " +
         std::to_string(NetResultDescription(result)) + ")";
}

#ifdef __SWITCH__
// The bit layout above is transcribed from switch/result.h. If libnx ever moves
// it, the decoded module and description in a user's log stop being true while
// still looking confident — the exact failure this decoder exists to prevent.
// So the Switch build checks the transcription against libnx's own macros, and
// the spec suite (which does not run on Switch) checks it here.
static_assert(NetResultModule(0x5A5Au) == R_MODULE(0x5A5Au),
              "NetResultModule must match libnx R_MODULE");
static_assert(NetResultDescription(0x5A5Au) == R_DESCRIPTION(0x5A5Au),
              "NetResultDescription must match libnx R_DESCRIPTION");
#endif

NetSocket::~NetSocket() { Close(); }

bool NetSocket::Open(uint16_t port) {
  // Close first, so a failed Open leaves the object in the state it claims:
  // the platform-init check used to sit above this and return false with a
  // previously-open socket still attached, which is a leak the caller has no
  // way to know about.
  Close();

  if (!NetSocketsInit()) {
    logger_.Err(
        NetSocketOpenFailureMessage(NetSocketOpenFailure::PlatformInit, port) +
        NetPlatformInitDetail());
    return false;
  }

  // The platform arms produce two outcomes and nothing else; the decision
  // about what they mean is NetClassifySocketSetup's, and it is spec'd there.
  // What this buys is that the #ifdef no longer straddles a condition and the
  // log line is written once.
  bool socketOk = false;
  bool nonBlockingOk = false;
#ifdef _WIN32
  fd_ = (int)socket(AF_INET, SOCK_DGRAM, 0);
  socketOk = fd_ != NET_INVALID_SOCKET;
  if (socketOk) {
    u_long nonBlocking = 1;
    nonBlockingOk = ioctlsocket((SOCKET)fd_, FIONBIO, &nonBlocking) == 0;
  }
#else
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  socketOk = fd_ != NET_INVALID_SOCKET;
  if (socketOk) {
    int flags = fcntl(fd_, F_GETFL, 0);
    nonBlockingOk =
        flags != -1 && fcntl(fd_, F_SETFL, flags | O_NONBLOCK) != -1;
  }
#endif
  const NetSocketOpenFailure failure =
      NetClassifySocketSetup(socketOk, nonBlockingOk);
  if (failure != NetSocketOpenFailure::None) {
    logger_.Err(NetSocketOpenFailureMessage(failure, port));
    Close();
    return false;
  }

#ifdef _WIN32
  // Windows turns an ICMP port-unreachable drawn by a previous sendto into a
  // WSAECONNRESET on the *next* recvfrom of this UDP socket. That is noise for
  // a connectionless socket — a peer that quit should not stop us reading from
  // everyone else — so switch it off. Best-effort: a stack that rejects the
  // ioctl just leaves the behaviour on, which Recv also handles.
  DWORD connReset = 0, ioctlBytes = 0;
  WSAIoctl((SOCKET)fd_, SIO_UDP_CONNRESET, &connReset, sizeof(connReset), NULL,
           0, &ioctlBytes, NULL, NULL);
#endif

  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(fd_, (const sockaddr *)&addr, sizeof(addr)) != 0) {
    logger_.Err(NetSocketOpenFailureMessage(NetSocketOpenFailure::Bind, port));
    Close();
    return false;
  }
  return true;
}

void NetSocket::Close() {
#ifdef _WIN32
  if (fd_ != -1)
    closesocket((SOCKET)fd_);
#else
  if (fd_ != -1)
    ::close(fd_);
#endif
  fd_ = -1;
}

bool NetSocket::Send(const NetAddress &addr, const uint8_t *data, int size) {
  if (fd_ == -1 || !data || size < 0)
    return false;
  sockaddr_in to;
  std::memset(&to, 0, sizeof(to));
  to.sin_family = AF_INET;
  to.sin_addr.s_addr = addr.ip;
  to.sin_port = addr.port;
#ifdef _WIN32
  int n = sendto((SOCKET)fd_, (const char *)data, size, 0,
                 (const sockaddr *)&to, sizeof(to));
#else
  int n = sendto(fd_, data, size, 0, (const sockaddr *)&to, sizeof(to));
#endif
  return n == size;
}

int NetSocket::Recv(NetAddress &addr, uint8_t *data, int maxSize) {
  if (fd_ == -1 || !data || maxSize <= 0)
    return -1;
  sockaddr_in from;
  socklen_t fromLen = sizeof(from);
#ifdef _WIN32
  // -1 means "nothing left to read" to both Poll drain loops, which break on
  // it. Two winsock errors would be misread as that and stall every remaining
  // datagram for the tick, so neither is allowed to reach the return below.
  // The bound only guards a stack that reports the same error forever; each
  // iteration otherwise consumes one queued datagram or notification.
  int n = -1;
  for (int attempt = 0; attempt < 8; attempt++) {
    fromLen = sizeof(from);
    n = recvfrom((SOCKET)fd_, (char *)data, maxSize, 0, (sockaddr *)&from,
                 &fromLen);
    if (n >= 0)
      break;
    int err = WSAGetLastError();
    if (err == WSAEMSGSIZE) {
      // The datagram was bigger than maxSize. Winsock consumed it and filled
      // the buffer, then reported an error; POSIX recvfrom returns maxSize for
      // the same case. Match POSIX so the caller sees one truncated packet.
      n = maxSize;
      break;
    }
    if (err == WSAECONNRESET || err == WSAENETRESET)
      continue; // stale ICMP unreachable, socket is still fine — read again
    if (!NetWouldBlock())
      logger_.Err("NetSocket: recvfrom failed");
    return -1;
  }
  if (n < 0)
    return -1;
#else
  int n = recvfrom(fd_, data, maxSize, 0, (sockaddr *)&from, &fromLen);
  if (n < 0) {
    if (!NetWouldBlock())
      logger_.Err("NetSocket: recvfrom failed");
    return -1;
  }
#endif
  addr.ip = from.sin_addr.s_addr;
  addr.port = from.sin_port;
  return n;
}

uint16_t NetSocket::GetBoundPort() const {
  if (fd_ == -1)
    return 0;
  sockaddr_in addr;
  socklen_t len = sizeof(addr);
  if (getsockname(fd_, (sockaddr *)&addr, &len) != 0)
    return 0;
  return ntohs(addr.sin_port);
}

NetAddress NetAddressFromParts(uint32_t ipHost, uint16_t portHost) {
  NetAddress addr;
  addr.ip = htonl(ipHost);
  addr.port = htons(portHost);
  return addr;
}

uint32_t NetIpToHost(const NetAddress &addr) { return ntohl(addr.ip); }

uint16_t NetPortToHost(const NetAddress &addr) { return ntohs(addr.port); }

std::string NetAddressToString(const NetAddress &addr) {
  // inet_ntop is a ws2_32 call, so it needs winsock up even though no socket
  // is involved. Logging an address before anything opens a socket is normal.
  (void)NetSocketsInit();
  char buf[INET_ADDRSTRLEN + 8];
  char ip[INET_ADDRSTRLEN];
  struct in_addr in;
  in.s_addr = addr.ip;
  inet_ntop(AF_INET, &in, ip, sizeof(ip));
  std::snprintf(buf, sizeof(buf), "%s:%u", ip, NetPortToHost(addr));
  return std::string(buf);
}

bool NetResolveAddress(const std::string &hostOrIp, uint16_t port,
                       NetAddress &out) {
  // getaddrinfo is a ws2_32 call and fails with WSANOTINITIALISED until
  // WSAStartup has run. NetClient::Connect resolves the host *before* it opens
  // its socket, so a client-only process reaches here with winsock still down
  // and every hostname — dotted-quad literals included, since no AI_NUMERICHOST
  // short-circuits the resolver — would fail as if the name were bad.
  if (!NetSocketsInit())
    return false;

  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; // IPv4 for now
  hints.ai_socktype = SOCK_DGRAM;

  struct addrinfo *results = nullptr;
  std::string portStr = std::to_string(port);
  if (getaddrinfo(hostOrIp.c_str(), portStr.c_str(), &hints, &results) != 0 ||
      !results)
    return false;

  bool ok = false;
  for (struct addrinfo *r = results; r; r = r->ai_next) {
    if (r->ai_family == AF_INET && r->ai_addrlen >= sizeof(sockaddr_in)) {
      const sockaddr_in *sin = (const sockaddr_in *)r->ai_addr;
      out.ip = sin->sin_addr.s_addr;
      out.port = sin->sin_port;
      ok = true;
      break;
    }
  }
  freeaddrinfo(results);
  return ok;
}

uint32_t NetNowMs() {
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

uint32_t NetRandom32() {
  // Reseed on first use, and on POSIX after fork(2) as well: a forked child
  // inherits the parent's state, so the pid check guarantees independent
  // streams. Windows has no fork, so _getpid() is fixed for the process
  // lifetime and the pid half of the test never fires there — the first-use
  // seeding is what does the work. Kept unconditional because the cost is one
  // comparison per call and a POSIX-only branch here would buy nothing.
  static uint64_t seed = 0;
  static uint32_t seedPid = 0;
  if (seed == 0 || seedPid != (uint32_t)NET_GETPID()) {
    // Mix a few independent sources; if any is weak or fails, the rest
    // still carry entropy (urandom > random_device > time + address).
    std::random_device rd;
    uint64_t s = ((uint64_t)rd() << 32) | rd();
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (urandom)
      urandom.read((char *)&s, (std::streamsize)sizeof(s));
    uint64_t t = NetNowMs();
    seed = s ^ (t << 32 | t) ^ (uint64_t)(uintptr_t)&seed ^
           ((uint64_t)(uint32_t)NET_GETPID() << 16) ^ 0x9E3779B97F4A7C15ULL;
    if (seed == 0)
      seed = 0x9E3779B97F4A7C15ULL;
    seedPid = (uint32_t)NET_GETPID();
  }
  // xorshift64
  seed ^= seed << 13;
  seed ^= seed >> 7;
  seed ^= seed << 17;
  return (uint32_t)seed;
}

} // namespace storm
