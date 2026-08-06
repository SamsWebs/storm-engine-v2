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

// ws2_32 refuses every entry point with WSANOTINITIALISED until WSAStartup has
// succeeded once in the process — name resolution included, not just socket().
// So every function here that touches winsock calls this first, and it has to
// be idempotent: the function-local static runs its initializer exactly once,
// on whichever call gets there first, and is thread-safe by construction.
static bool NetSocketsInit() {
#ifdef _WIN32
  static const bool ok = [] {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
  }();
  return ok;
#else
  return true;
#endif
}

static bool NetWouldBlock() {
#ifdef _WIN32
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EWOULDBLOCK || errno == EAGAIN;
#endif
}

NetSocket::~NetSocket() { Close(); }

bool NetSocket::Open(uint16_t port) {
  if (!NetSocketsInit())
    return false;
  Close();

#ifdef _WIN32
  fd_ = (int)socket(AF_INET, SOCK_DGRAM, 0);
  u_long nonBlocking = 1;
  if (fd_ == NET_INVALID_SOCKET ||
      ioctlsocket((SOCKET)fd_, FIONBIO, &nonBlocking) != 0) {
#else
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  int flags = fcntl(fd_, F_GETFL, 0);
  if (fd_ == NET_INVALID_SOCKET || flags == -1 ||
      fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
#endif
    logger_.Err("NetSocket: failed to create non-blocking UDP socket");
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
    logger_.Err("NetSocket: failed to bind UDP port " + std::to_string(port));
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
