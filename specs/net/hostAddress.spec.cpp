// Host address enumeration — 2.7.1.
//
// A net layer that cannot tell the host its own LAN address leaves every game
// to solve it, and the game that hit this wrote it per platform (getifaddrs /
// getaddrinfo / a Switch guard) and then had to split the RANKING rule out
// separately because that was the part worth testing. This file is that split:
// the rule is pure and spec'd, the enumeration is a thin platform layer over
// it.
//
// The reason the ranking needs a rule at all, rather than "return the first
// non-loopback": every machine has several, and they are not equally useful.
// A laptop is typically carrying a 127.0.0.1, a docker0 at 172.17.0.1, a
// veth pair, a VPN tunnel and exactly one interface a player on the same
// network can reach. Returning the docker bridge gets the host a lobby full of
// addresses nobody can connect to, and the symptom -- "it works on my machine"
// -- points nowhere near the cause.
#include <igloo/igloo_alt.h>

#include <string>
#include <vector>

#include "../../common/net/hostAddress.h"

using namespace igloo;
using namespace storm;

namespace {

NetCandidate Make(const std::string &name, const std::string &address) {
  NetCandidate c;
  c.interfaceName = name;
  c.address = address;
  return c;
}

} // namespace

Describe(HostAddressSpec) {

  // ── Classification ─────────────────────────────────────────────────────────

  It(recognises_loopback_and_nothing_else_as_loopback) {
    Assert::That(IsLoopbackV4("127.0.0.1"), Equals(true));
    Assert::That(IsLoopbackV4("127.1.2.3"), Equals(true)); // all of 127/8
    Assert::That(IsLoopbackV4("10.0.0.5"), Equals(false));
    Assert::That(IsLoopbackV4("192.168.1.10"), Equals(false));
    Assert::That(IsLoopbackV4(""), Equals(false));
  };

  It(recognises_link_local_and_treats_it_as_a_last_resort) {
    // 169.254/16 is what a DHCP client gets when no DHCP server answered. It is
    // a real address and it is on the local link, so it beats nothing -- but
    // it must never outrank a real LAN address, because a self-assigned one is
    // exactly the "nobody can connect to me" case the ranking exists to avoid.
    Assert::That(IsLinkLocalV4("169.254.10.20"), Equals(true));
    Assert::That(IsLinkLocalV4("169.253.10.20"), Equals(false));
    Assert::That(IsLinkLocalV4("10.0.0.1"), Equals(false));
  };

  It(classifies_by_subnet_so_a_docker_bridge_never_outranks_a_lan_card) {
    // The case that makes a first-non-loopback rule wrong. docker0 is
    // 172.17.0.1 and answers first on most Linux machines; eth0 is
    // 192.168.1.x and is the one a player can actually reach.
    NetCandidate docker = Make("docker0", "172.17.0.1");
    NetCandidate eth = Make("eth0", "192.168.1.42");
    docker.isVirtual = true;
    Assert::That(Classify(docker), Equals(AddressClass::Virtual));
    Assert::That(Classify(eth), Equals(AddressClass::Private));
  };

  It(treats_a_wireless_interface_as_better_than_a_wired_one) {
    // Not obvious, and worth stating: on a desktop the wired card is the one
    // a player on the LAN can reach, and on a laptop the wireless one is.
    // Neither is universally right, so this is a TIE-BREAK below the subnet
    // decision, not a rule that overrides it.
    NetCandidate wired = Make("eth0", "192.168.1.42");
    NetCandidate wireless = Make("wlan0", "192.168.1.77");
    wireless.isWireless = true;
    Assert::That(BetterThan(wireless, wired), Equals(true));
    Assert::That(BetterThan(wired, wireless), Equals(false));
  };

  It(keeps_the_wireless_nudge_inside_a_class_and_never_across_one) {
    // The nudge is a tie-break WITHIN a class. Given +1000 it would let a
    // wireless DOCKER bridge outrank a wired LAN card, which is the exact
    // failure the virtual class exists to prevent wearing a different hat.
    NetCandidate wirelessVirtual = Make("wlp3s0", "172.17.0.1");
    wirelessVirtual.isWireless = true;
    wirelessVirtual.isVirtual = true;
    const NetCandidate wiredPrivate = Make("eth0", "192.168.1.42");

    Assert::That(Classify(wirelessVirtual), Equals(AddressClass::Virtual));
    Assert::That(BetterThan(wiredPrivate, wirelessVirtual), Equals(true));
    // And within the same class the nudge does apply, or the earlier case
    // asserting wireless-beats-wired would be asserting nothing.
    NetCandidate wirelessPrivate = Make("wlan0", "192.168.1.77");
    wirelessPrivate.isWireless = true;
    Assert::That(BetterThan(wirelessPrivate, wiredPrivate), Equals(true));
  };

  It(never_ranks_a_candidate_with_no_address_at_all) {
    // An interface can be up with no address yet -- a dongle, a VPN still
    // associating. If such a candidate can be ranked into a lobby the host
    // hands out an empty string. Asserted as "no ranked output ever contains
    // one", because asserting only that a good candidate wins is a test that
    // passes whenever the bad one happens to sort lower by luck.
    std::vector<NetCandidate> all;
    all.push_back(Make("eth0", ""));
    all.push_back(Make("lo", "127.0.0.1"));
    all.push_back(Make("docker0", "172.17.0.1"));

    const std::vector<NetCandidate> ranked = Ranked(all);
    for (const NetCandidate &c : ranked) {
      Assert::That(c.address.empty(), Equals(false));
    }
    // And the blank one is never the answer. Note that the answer here is the
    // DOCKER address and not "nothing": a virtual interface is a real,
    // connectable address, and a game deliberately running on a container
    // network wants it. An earlier version of this case asserted the result
    // was empty, which would have meant treating every virtual interface as
    // unusable -- the opposite of what the Virtual class is for.
    const NetCandidate best = ChooseBest(all);
    Assert::That(best.address, Equals("172.17.0.1"));
  };

  It(rank_loopback_worst_and_a_real_lan_address_best) {
    const NetCandidate loop = Make("lo", "127.0.0.1");
    const NetCandidate linkLocal = Make("eth0", "169.254.3.4");
    const NetCandidate priv = Make("eth0", "192.168.1.42");
    const NetCandidate pub = Make("eth0", "8.8.8.8");

    Assert::That(Score(loop) < Score(linkLocal), Equals(true));
    Assert::That(Score(linkLocal) < Score(priv), Equals(true));
    // A public address is a machine on the internet, not a LAN lobby, so it
    // must not win by default -- it is usually the machine's WAN side, which
    // no player on the sofa can reach.
    Assert::That(Score(pub) <= Score(priv), Equals(true));
  };

  // ── Choosing ───────────────────────────────────────────────────────────────

  It(picks_the_best_from_an_unordered_list) {
    // Unordered on purpose: getifaddrs returns interfaces in kernel order,
    // which is not a ranking and changes between boots and machines.
    std::vector<NetCandidate> all;
    all.push_back(Make("lo", "127.0.0.1"));
    all.push_back(Make("docker0", "172.17.0.1"));
    all.push_back(Make("eth0", "192.168.1.42"));
    all.push_back(Make("wlan0", "192.168.1.77"));
    all[1].isVirtual = true;
    all[3].isWireless = true;

    const NetCandidate best = ChooseBest(all);
    // Wireless beats wired inside the same subnet class; docker and loopback
    // are out.
    Assert::That(best.interfaceName, Equals("wlan0"));
  };

  It(returns_nothing_rather_than_a_loopback_when_that_is_all_there_is) {
    // A host with only a loopback interface has no address to offer. Handing
    // back 127.0.0.1 is how a lobby ends up advertising an address that only
    // works on the host.
    std::vector<NetCandidate> onlyLoop;
    onlyLoop.push_back(Make("lo", "127.0.0.1"));
    const NetCandidate best = ChooseBest(onlyLoop);
    Assert::That(best.interfaceName.empty(), Equals(true));
    Assert::That(best.address.empty(), Equals(true));
  };

  It(returns_nothing_for_an_empty_list) {
    const NetCandidate best = ChooseBest({});
    Assert::That(best.address.empty(), Equals(true));
  };

  It(ignores_a_candidate_with_no_address) {
    // An interface can be up with no address yet -- a dongle, a VPN still
    // associating. Ranking it as a real address hands the host an empty string
    // to put in a lobby.
    std::vector<NetCandidate> all;
    all.push_back(Make("eth0", ""));
    all.push_back(Make("eth1", "192.168.1.9"));
    const NetCandidate best = ChooseBest(all);
    Assert::That(best.address, Equals("192.168.1.9"));
  };

  It(is_order_independent) {
    // The property that makes the rule worth having: the answer cannot depend
    // on the order the OS happened to hand the list over in.
    std::vector<NetCandidate> a;
    a.push_back(Make("lo", "127.0.0.1"));
    a.push_back(Make("eth0", "192.168.1.42"));
    std::vector<NetCandidate> b;
    b.push_back(Make("eth0", "192.168.1.42"));
    b.push_back(Make("lo", "127.0.0.1"));
    Assert::That(ChooseBest(a).address, Equals(ChooseBest(b).address));
  };

  It(offers_every_usable_address_when_a_host_has_several_lan_cards) {
    // Choosing ONE is right for a default, and wrong for a lobby: a player on
    // a different subnet may only be able to reach the second card. So both
    // exist, and Ranked() is the ordered form a UI lists.
    std::vector<NetCandidate> all;
    all.push_back(Make("eth0", "10.0.0.5"));
    all.push_back(Make("eth1", "192.168.1.42"));
    all.push_back(Make("lo", "127.0.0.1"));
    const std::vector<NetCandidate> ranked = Ranked(all);
    Assert::That(ranked.size(), Equals(static_cast<std::size_t>(2)));
    // Both survive and loopback does not. Which of the TWO comes first is
    // deliberately NOT asserted: 10.0.0.5 and 192.168.1.42 are the same class
    // with the same flags, and there is no honest rule that separates them.
    // An earlier version of this case expected 192.168 first, which would have
    // meant encoding a guess -- "192.168 is the home-router range" -- as
    // policy, on the theory that developer machines are the ones testing.
    // A host whose LAN is 10.x is just as real.
    bool hasTen = false;
    bool has192 = false;
    for (const NetCandidate &c : ranked) {
      if (c.address == "10.0.0.5") {
        hasTen = true;
      }
      if (c.address == "192.168.1.42") {
        has192 = true;
      }
    }
    Assert::That(hasTen, Equals(true));
    Assert::That(has192, Equals(true));
  };

  It(parses_an_address_without_a_platform_getifaddrs) {
    // Parsed, not validated by the kernel, so the ranking is testable with no
    // network at all. A string the kernel would reject still classifies by its
    // first octet, which is all the ranking ever looks at.
    NetCandidate c;
    c.address = "192.168.0.1";
    Assert::That(FirstOctet(c.address), Equals(192));
    Assert::That(FirstOctet("not-an-address"), Equals(-1));
  };
};
