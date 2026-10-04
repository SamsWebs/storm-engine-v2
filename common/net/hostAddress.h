#pragma once

// Telling the host its own LAN address, and ranking the answers.
//
// 2.7.1. A net layer that cannot answer "what is my address on the network my
// players are on" leaves every game to solve it, and the game that hit this
// wrote it per platform (getifaddrs / getaddrinfo / a Switch guard) and then
// had to split the RANKING rule out separately -- because the ranking was the
// part worth testing and the enumeration was the part that needed a socket.
//
// So the split is the same one the rest of the engine uses: the RULE is pure,
// header-only and spec'd; the ENUMERATION is a thin platform layer over it.
//
// WHY A RANKING RULE AND NOT "THE FIRST NON-LOOPBACK"
//
// Every machine has several addresses and they are not equally useful. A
// developer laptop typically carries 127.0.0.1, a docker0 at 172.17.0.1, a veth
// pair, a VPN tunnel, and exactly one interface a player on the same network
// can reach. Returning the docker bridge -- which is what "first
// non-loopback" does on most Linux machines, because getifaddrs returns
// interfaces in kernel order -- puts an address in the lobby that nobody can
// connect to. The symptom is "it works on my machine", and it points nowhere
// near the cause.
//
// Header-only and exception-free. No .cpp in common/net/ for this, so the
// Switch and Android source lists do not need a new entry for it.
//
// A NOTE ON THE NAMES, because they are free functions in a namespace that
// predates the namespace: Score, Ranked and Classify are short and generic,
// and <stormengine2/compat/global.h> re-exports every public engine name into
// the global namespace, so a 1.x game that force-includes the bridge will have
// its own `Score` collide with this one. That is the bridge's documented cost
// and it is why the bridge is meant to be deleted, but it is worth naming here
// rather than discovering at link time: a game that keeps the bridge and has a
// `Score` should qualify this one as `storm::Score`.
#include <algorithm>
#include <string>
#include <vector>

namespace storm {

// One interface and the address on it. A plain struct, filled in by whatever
// the platform layer found, so the ranking never has to know where a candidate
// came from.
struct NetCandidate {
  std::string interfaceName; // "eth0", "wlan0", "en0"
  std::string address;       // dotted quad, empty when the interface has none
  bool isVirtual = false;    // docker, veth, bridges, tunnels
  bool isWireless = false;   // wlan, wifi, en0 on a Mac
};

// Broad classes, in the order a host should prefer them.
enum class AddressClass { Loopback, LinkLocal, Virtual, Private, Public };

// The first octet, or -1 for anything that is not a dotted quad. Parsed rather
// than validated by the kernel, because the ranking only ever looks at this
// and a spec with no network is worth more than one that needs a live NIC.
inline int FirstOctet(const std::string &address) {
  int value = 0;
  int digits = 0;
  for (char c : address) {
    if (c == '.') {
      break;
    }
    if (c < '0' || c > '9' || digits >= 3) {
      return -1;
    }
    value = value * 10 + (c - '0');
    ++digits;
  }
  return digits > 0 && value <= 255 ? value : -1;
}

// 127.0.0.0/8 is the whole block, not just 127.0.0.1.
inline bool IsLoopbackV4(const std::string &address) {
  return FirstOctet(address) == 127;
}

// 169.254.0.0/16: what a DHCP client self-assigns when no server answered.
// Real, on the local link, and a last resort -- it must never outrank a real
// LAN address, because that is the same "nobody can reach me" failure as the
// docker bridge, wearing a plausible address.
inline bool IsLinkLocalV4(const std::string &address) {
  if (FirstOctet(address) != 169) {
    return false;
  }
  // The SECOND octet has to be 254. An earlier version of this walked the
  // string and stopped at the first dot, which meant it re-parsed 169 and
  // compared THAT to 254 -- so every link-local address classified as private
  // and the rule silently did nothing. Read past the dot instead.
  const std::size_t dot = address.find('.');
  if (dot == std::string::npos) {
    return false;
  }
  int value = 0;
  int digits = 0;
  for (std::size_t i = dot + 1; i < address.size(); ++i) {
    const char c = address[i];
    if (c == '.') {
      break;
    }
    if (c < '0' || c > '9' || digits >= 3) {
      return false;
    }
    value = value * 10 + (c - '0');
    ++digits;
  }
  return digits > 0 && value == 254;
}

inline bool IsPrivateV4(const std::string &address) {
  const int first = FirstOctet(address);
  return first == 10 || first == 192 || first == 172;
}

inline AddressClass Classify(const NetCandidate &candidate) {
  if (IsLoopbackV4(candidate.address)) {
    return AddressClass::Loopback;
  }
  if (candidate.isVirtual) {
    return AddressClass::Virtual;
  }
  if (IsLinkLocalV4(candidate.address)) {
    return AddressClass::LinkLocal;
  }
  if (IsPrivateV4(candidate.address)) {
    return AddressClass::Private;
  }
  return AddressClass::Public;
}

// Higher is better. Deliberately coarse and deliberately readable: this is a
// ranking a developer will need to reason about from a bug report, and a table
// of magic constants with no order would be worse than none.
//
// Public ranks BELOW private on purpose. A public address on an interface is
// usually the machine's WAN side or a VPN endpoint, and the player sitting on
// the same sofa usually cannot reach either. A host with only a public address
// is rare; a host whose only *reachable* address is private is the norm.
inline int Score(const NetCandidate &candidate) {
  int score = 0;
  switch (Classify(candidate)) {
  case AddressClass::Loopback:
    return 0;
  case AddressClass::Virtual:
    score = 10;
    break;
  case AddressClass::LinkLocal:
    score = 20;
    break;
  case AddressClass::Private:
    score = 100;
    break;
  case AddressClass::Public:
    score = 50;
    break;
  }
  if (candidate.isWireless) {
    // A tie-break WITHIN a class, never across one: on a desktop the wired
    // card is the reachable one, on a laptop the wireless one is. Neither is
    // universally right, so this nudges rather than decides.
    score += 5;
  }
  return score;
}

inline bool BetterThan(const NetCandidate &a, const NetCandidate &b) {
  return Score(a) > Score(b);
}

// A candidate with no address is not a candidate: an interface can be up with
// none yet (a dongle, a VPN still associating), and ranking it hands the host
// an empty string to put in a lobby.
inline bool IsUsable(const NetCandidate &candidate) {
  return !candidate.address.empty() && FirstOctet(candidate.address) >= 0 &&
         Classify(candidate) != AddressClass::Loopback;
}

// Every usable candidate, best first. This is what a lobby UI should list: a
// player on a different subnet may only be able to reach the second card, so
// "one default" is the wrong answer for a list and the right one for a default.
inline std::vector<NetCandidate> Ranked(const std::vector<NetCandidate> &all) {
  std::vector<NetCandidate> usable;
  for (const NetCandidate &candidate : all) {
    if (IsUsable(candidate)) {
      usable.push_back(candidate);
    }
  }
  std::stable_sort(usable.begin(), usable.end(),
                   [](const NetCandidate &a, const NetCandidate &b) {
                     return Score(a) > Score(b);
                   });
  return usable;
}

// The single best candidate, or an EMPTY one when there is none. Never returns
// loopback: a host with only a loopback interface has no address to offer, and
// handing back 127.0.0.1 is how a lobby advertises an address that works only
// on the host.
inline NetCandidate ChooseBest(const std::vector<NetCandidate> &all) {
  const std::vector<NetCandidate> ranked = Ranked(all);
  return ranked.empty() ? NetCandidate{} : ranked.front();
}

// ── The platform half ────────────────────────────────────────────────────────
//
// Declared here so a consumer includes ONE header, and DEFINED in
// hostAddress.cpp because getifaddrs is a system call and putting <ifaddrs.h>
// behind every consumer of the ranking above would defeat the split. That .cpp
// is in engine-sources.txt, so it reaches the Switch and Android builds too --
// and note that these two are the only functions in this header that are not
// header-only, which is the point of the arrangement and not an oversight.
//
// LocalCandidates() returns the RAW candidates, unranked. Ranking is the
// caller's decision, and returning a ranked list from here would put the
// opinions back on the side that needs a socket.
std::vector<NetCandidate> LocalCandidates();

// The one a lobby should offer by default, or an EMPTY candidate when there is
// nothing to offer. Never loopback.
NetCandidate BestLocalAddress();

} // namespace storm
