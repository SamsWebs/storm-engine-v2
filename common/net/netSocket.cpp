#include <chrono>
#include <cstdio>
#include <cstring>

#include "netSocket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = int;
#define NET_INVALID_SOCKET INVALID_SOCKET
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
#endif

static bool NetSocketsInit() {
#ifdef _WIN32
    static bool done = false;
    static bool ok = false;
    if (!done) {
        done = true;
        WSADATA wsa;
        ok = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }
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

NetSocket::~NetSocket() {
    Close();
}

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
    int n = sendto((SOCKET)fd_, (const char *)data, size, 0, (const sockaddr *)&to, sizeof(to));
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
    int n = recvfrom((SOCKET)fd_, (char *)data, maxSize, 0, (sockaddr *)&from, &fromLen);
#else
    int n = recvfrom(fd_, data, maxSize, 0, (sockaddr *)&from, &fromLen);
#endif
    if (n < 0) {
        if (!NetWouldBlock())
            logger_.Err("NetSocket: recvfrom failed");
        return -1;
    }
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

uint32_t NetIpToHost(const NetAddress &addr) {
    return ntohl(addr.ip);
}

uint16_t NetPortToHost(const NetAddress &addr) {
    return ntohs(addr.port);
}

std::string NetAddressToString(const NetAddress &addr) {
    char buf[INET_ADDRSTRLEN + 8];
    char ip[INET_ADDRSTRLEN];
    struct in_addr in;
    in.s_addr = addr.ip;
    inet_ntop(AF_INET, &in, ip, sizeof(ip));
    std::snprintf(buf, sizeof(buf), "%s:%u", ip, NetPortToHost(addr));
    return std::string(buf);
}

bool NetResolveAddress(const std::string &hostOrIp, uint16_t port, NetAddress &out) {
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4 for now
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo *results = nullptr;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(hostOrIp.c_str(), portStr.c_str(), &hints, &results) != 0 || !results)
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
    static uint64_t seed = 0;
    if (seed == 0) {
        seed = (uint64_t)NetNowMs() ^ ((uint64_t)(uintptr_t)&seed) ^ 0x9E3779B97F4A7C15ULL;
    }
    // xorshift64
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return (uint32_t)seed;
}
