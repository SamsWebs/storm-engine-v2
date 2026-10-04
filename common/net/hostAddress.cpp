// Host address enumeration, per platform. The RANKING lives in
// hostAddress.h, which is pure; this file is the thin part that needs a
// socket.
//
// Split deliberately: the enumeration needs a live network and is awkward to
// test, and the ranking is the part with the opinions in it. Putting them in
// one file means the opinions arrive untested.
//
// A .cpp, unlike the rest of this slice, because getifaddrs is a system call
// and header-only would put <ifaddrs.h> behind every consumer of the ranking.
// It is listed in engine-sources.txt, so it reaches the Switch and Android
// builds too.
#include "hostAddress.h"

#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace storm {
namespace {

// An interface name worth showing in a lobby, as opposed to a kernel index.
// Prefix matching rather than an allow-list: a new wireless driver naming
// itself something unexpected should still be recognised as wireless, and the
// cost of a false positive is a tie-break, not a wrong address.
bool LooksVirtual(const char *name) {
  static const char *kVirtualPrefixes[] = {
      "docker", "veth", "br-", "virbr", "vmnet", "tun",  "tap",
      "utun",   "ham",  "wg",  "zt",    "lxc",   "dummy"};
  for (const char *prefix : kVirtualPrefixes) {
    if (std::strncmp(name, prefix, std::strlen(prefix)) == 0) {
      return true;
    }
  }
  return false;
}

bool LooksWireless(const char *name) {
  static const char *kWirelessPrefixes[] = {"wl", "wifi", "wlan", "en", "ath"};
  for (const char *prefix : kWirelessPrefixes) {
    if (std::strncmp(name, prefix, std::strlen(prefix)) == 0) {
      return true;
    }
  }
  return false;
}

} // namespace

// Every IPv4 address on this machine's interfaces, as raw candidates.
//
// NOT ranked. Ranked() and ChooseBest() are the callers' decision, and
// returning a ranked list from here would put the opinions back on the side
// that needs a socket.
//
// Returns an empty vector rather than a loopback entry when the platform call
// fails or the machine genuinely has no usable interface: ChooseBest() then
// returns an empty candidate, which a lobby can report as "no address found"
// instead of advertising 127.0.0.1.
std::vector<NetCandidate> LocalCandidates() {
  std::vector<NetCandidate> found;

#if defined(_WIN32)
  // Winsock, not getifaddrs, which MSVC does not have. The host name is
  // resolved rather than the interfaces enumerated: there is no supported way
  // to walk adapters without iphlpapi, and resolving the hostname is what a
  // Windows LAN lobby can actually use.
  char host[256];
  if (gethostname(host, sizeof(host)) != 0) {
    return found;
  }
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  addrinfo *result = nullptr;
  if (getaddrinfo(host, nullptr, &hints, &result) != 0 || result == nullptr) {
    return found;
  }
  for (addrinfo *it = result; it != nullptr; it = it->ai_next) {
    if (it->ai_family != AF_INET || it->ai_addr == nullptr) {
      continue;
    }
    char text[INET_ADDRSTRLEN] = {0};
    const sockaddr_in *addr =
        reinterpret_cast<const sockaddr_in *>(it->ai_addr);
    if (inet_ntop(AF_INET, &addr->sin_addr, text, sizeof(text)) == nullptr) {
      continue;
    }
    NetCandidate candidate;
    candidate.interfaceName = "local";
    candidate.address = text;
    found.push_back(candidate);
  }
  freeaddrinfo(result);
  return found;
#else
  ifaddrs *list = nullptr;
  if (getifaddrs(&list) != 0 || list == nullptr) {
    return found;
  }
  for (ifaddrs *it = list; it != nullptr; it = it->ifa_next) {
    if (it->ifa_addr == nullptr || it->ifa_addr->sa_family != AF_INET) {
      continue; // IPv6 and address-less interfaces are out of scope
    }
    char text[INET_ADDRSTRLEN] = {0};
    const sockaddr_in *addr =
        reinterpret_cast<const sockaddr_in *>(it->ifa_addr);
    if (inet_ntop(AF_INET, &addr->sin_addr, text, sizeof(text)) == nullptr) {
      continue;
    }
    NetCandidate candidate;
    candidate.interfaceName = it->ifa_name != nullptr ? it->ifa_name : "";
    candidate.address = text;
    candidate.isVirtual = LooksVirtual(candidate.interfaceName.c_str());
    candidate.isWireless = LooksWireless(candidate.interfaceName.c_str());
    found.push_back(candidate);
  }
  freeifaddrs(list);
  return found;
#endif
}

// The one a lobby should offer by default. Empty when there is nothing to
// offer -- see the note on LocalCandidates().
NetCandidate BestLocalAddress() { return ChooseBest(LocalCandidates()); }

} // namespace storm
