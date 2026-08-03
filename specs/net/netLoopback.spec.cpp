#include <chrono>
#include <cstring>
#include <functional>
#include <set>
#include <string>
#include <thread>

#include <igloo/igloo_alt.h>

#include "../../common/net/netClient.h"
#include "../../common/net/netPacket.h"
#include "../../common/net/netServer.h"

using namespace igloo;

// End-to-end specs: a real NetServer and NetClient on loopback UDP sockets.
// The handshake, message routing, and disconnect flows are exercised through
// the actual OS sockets (the only layer the unit specs cannot cover).

namespace {
const int kPumpDeadlineMs = 3000;
const int kPumpStepMs = 1;

bool PumpUntil(NetServer &server, NetClient &client, int timeoutMs,
               std::function<bool()> done) {
  uint32_t start = NetNowMs();
  while (NetNowMs() - start < (uint32_t)timeoutMs) {
    server.Update();
    server.Poll();
    client.Update();
    client.Poll();
    if (done())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(kPumpStepMs));
  }
  return false;
}

int32_t RecvInt(const NetChunk &chunk) {
  int32_t v = 0;
  if (chunk.size == (int)sizeof(v))
    std::memcpy(&v, chunk.data, sizeof(v));
  return v;
}

struct Rendezvous {
  bool serverConnected = false;
  bool clientConnected = false;
  int clientId = -1;
  NetServer server;
  NetClient client;

  Rendezvous(uint16_t port) {
    server.SetOnClientConnect([this](int id) {
      serverConnected = true;
      clientId = id;
    });
    client.SetOnConnect([this]() { clientConnected = true; });
    Assert::That(server.Start(port, 8), Equals(true));
    Assert::That(client.Connect("127.0.0.1", server.GetPort()), Equals(true));
  }

  // Pumps until both sides report the connection established.
  bool Connect() {
    return PumpUntil(server, client, kPumpDeadlineMs,
                     [this]() { return serverConnected && clientConnected; });
  }
};

// A hand-driven peer: a bare socket that speaks control datagrams at a real
// NetServer. PumpUntil above drives two engine peers against each other; this
// drives the server against traffic a NetClient would never send, which is
// what the handshake has to survive.
struct RawPeer {
  NetSocket sock;
  NetAddress serverAddr;
  uint8_t clientNonce[4] = {0xDE, 0xAD, 0xBE, 0xEF};

  RawPeer(NetServer &server, uint8_t tag) {
    Assert::That(sock.Open(0), Equals(true));
    serverAddr = NetAddressFromParts(0x7F000001u, server.GetPort());
    clientNonce[3] = tag;
  }

  bool Send(int message, const void *payload, int payloadSize) {
    return NetSendControl(sock, serverAddr, message, payload, payloadSize);
  }

  // Pumps the server and waits for one control datagram of the given kind,
  // skipping keepalives and retransmissions of earlier steps.
  bool Await(NetServer &server, int message, NetControlPacket &out) {
    uint32_t start = NetNowMs();
    while (NetNowMs() - start < (uint32_t)kPumpDeadlineMs) {
      server.Update();
      server.Poll();
      NetAddress from;
      uint8_t buf[kNetMaxPacketSize];
      int n = sock.Recv(from, buf, sizeof(buf));
      if (n > 0 && NetControlPacket::IsControl(buf, n) && out.Unpack(buf, n) &&
          out.message == message)
        return true;
      if (n < 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(kPumpStepMs));
    }
    return false;
  }
};
} // namespace

Describe(NetLoopbackSpec) {

  It(should_complete_the_handshake_over_loopback) {
    Rendezvous r(0); // ephemeral server port
    Assert::That(r.Connect(), Equals(true));
    Assert::That(r.server.GetClientCount(), Equals(1));
    Assert::That(r.client.IsConnected(), Equals(true));
    Assert::That(r.client.GetServerAddress().port != 0, Equals(true));
  };

  It(should_exchange_reliable_messages_both_ways) {
    Rendezvous r(0);
    Assert::That(r.Connect(), Equals(true));

    int32_t serverSeen = 0, clientSeen = 0;
    r.server.SetOnChunk([&](int clientId, const NetChunk &chunk) {
      serverSeen = RecvInt(chunk);
    });
    r.client.SetOnChunk(
        [&](const NetChunk &chunk) { clientSeen = RecvInt(chunk); });

    int32_t toClient = 12345;
    Assert::That(r.server.Send(r.clientId, &toClient, sizeof(toClient), true),
                 Equals(true));
    Assert::That(PumpUntil(r.server, r.client, kPumpDeadlineMs,
                           [&]() { return clientSeen == 12345; }),
                 Equals(true));

    int32_t toServer = -6789;
    Assert::That(r.client.Send(&toServer, sizeof(toServer), true),
                 Equals(true));
    Assert::That(PumpUntil(r.server, r.client, kPumpDeadlineMs,
                           [&]() { return serverSeen == -6789; }),
                 Equals(true));
  };

  It(should_exchange_unreliable_messages) {
    Rendezvous r(0);
    Assert::That(r.Connect(), Equals(true));

    int32_t clientSeen = 0;
    r.client.SetOnChunk(
        [&](const NetChunk &chunk) { clientSeen = RecvInt(chunk); });

    int32_t ping = 42;
    Assert::That(r.server.Send(r.clientId, &ping, sizeof(ping), false),
                 Equals(true));
    Assert::That(PumpUntil(r.server, r.client, kPumpDeadlineMs,
                           [&]() { return clientSeen == 42; }),
                 Equals(true));
  };

  It(should_report_a_measured_rtt) {
    Rendezvous r(0);
    Assert::That(r.Connect(), Equals(true));

    int32_t serverSeen = 0, clientSeen = 0;
    r.server.SetOnChunk([&](int clientId, const NetChunk &chunk) {
      serverSeen = RecvInt(chunk);
    });
    r.client.SetOnChunk(
        [&](const NetChunk &chunk) { clientSeen = RecvInt(chunk); });

    // First round trip: on loopback the echo can return in the same
    // millisecond it was sent, so the initial RTT sample is 0.
    int32_t a = 7;
    r.client.Send(&a, sizeof(a), true);
    Assert::That(PumpUntil(r.server, r.client, kPumpDeadlineMs,
                           [&]() { return serverSeen == 7; }),
                 Equals(true));
    r.server.Send(r.clientId, &a, sizeof(a), true);
    Assert::That(PumpUntil(r.server, r.client, kPumpDeadlineMs,
                           [&]() { return clientSeen == 7; }),
                 Equals(true));

    // Second round trip: the server's ack rides on its later flush
    // cadence, so this sample is a real, non-zero RTT.
    int32_t b = 8;
    r.client.Send(&b, sizeof(b), true);
    Assert::That(PumpUntil(r.server, r.client, kPumpDeadlineMs,
                           [&]() { return serverSeen == 8; }),
                 Equals(true));
    Assert::That(PumpUntil(r.server, r.client, kPumpDeadlineMs,
                           [&]() { return r.client.GetRTT() > 0; }),
                 Equals(true));
  };

  It(should_notify_the_client_when_the_server_kicks_it) {
    Rendezvous r(0);
    Assert::That(r.Connect(), Equals(true));

    std::string reason;
    r.client.SetOnDisconnect([&](const std::string &why) { reason = why; });
    r.server.DisconnectClient(r.clientId, "bye");
    Assert::That(PumpUntil(r.server, r.client, kPumpDeadlineMs,
                           [&]() { return !reason.empty(); }),
                 Equals(true));
    Assert::That(reason, Equals("bye"));
    Assert::That(r.client.IsConnected(), Equals(false));
    Assert::That(r.server.GetClientCount(), Equals(0));
  };

  It(should_truncate_an_overlong_kick_reason_without_overflowing) {
    Rendezvous r(0);
    Assert::That(r.Connect(), Equals(true));

    std::string reason;
    r.client.SetOnDisconnect([&](const std::string &why) { reason = why; });

    // A reason far longer than a control packet's payload must not smash
    // the stack (P3); the client should still be notified immediately,
    // with the reason truncated to the wire limit.
    std::string huge(kNetMaxPayload + 2000, 'x');
    r.server.DisconnectClient(r.clientId, huge);
    Assert::That(PumpUntil(r.server, r.client, kPumpDeadlineMs,
                           [&]() { return !reason.empty(); }),
                 Equals(true));
    // The client strips the trailing wire byte (its NUL convention), so
    // it sees exactly kNetMaxPayload - 1 characters.
    Assert::That(reason, Equals(std::string(kNetMaxPayload - 1, 'x')));
    Assert::That(r.client.IsConnected(), Equals(false));
    Assert::That(r.server.GetClientCount(), Equals(0));
  };

  It(should_tell_a_client_when_the_server_is_full) {
    NetServer server;
    Assert::That(server.Start(0, 1), Equals(true));

    NetClient first;
    first.SetOnConnect([&]() {});
    Assert::That(first.Connect("127.0.0.1", server.GetPort()), Equals(true));

    NetClient second;
    std::string secondReason;
    second.SetOnDisconnect([&](const std::string &why) { secondReason = why; });
    Assert::That(second.Connect("127.0.0.1", server.GetPort()), Equals(true));

    bool firstConnected = false;
    first.SetOnConnect([&]() { firstConnected = true; });
    Assert::That(PumpUntil(server, first, kPumpDeadlineMs,
                           [&]() { return firstConnected; }),
                 Equals(true));
    Assert::That(PumpUntil(server, second, kPumpDeadlineMs,
                           [&]() { return secondReason == "server full"; }),
                 Equals(true));
    Assert::That(second.IsConnected(), Equals(false));
    Assert::That(server.GetClientCount(), Equals(1));
  };

  It(should_reuse_a_slots_nonce_when_connect_is_repeated) {
    // Re-sending CONNECT is normal (the client does it every retry until
    // CONNECT_ACCEPT lands). Minting a fresh nonce each time handed anyone
    // an unlimited supply of samples from the server's generator (P9), so
    // the slot must answer with the nonce it already issued.
    NetServer server;
    Assert::That(server.Start(0, 4), Equals(true));
    RawPeer peer(server, 0x11);

    NetControlPacket first;
    Assert::That(peer.Send(kNetControlConnect, peer.clientNonce, 4),
                 Equals(true));
    Assert::That(peer.Await(server, kNetControlConnectAccept, first),
                 Equals(true));
    Assert::That(first.payloadSize, Equals(4));
    uint8_t issued[4];
    std::memcpy(issued, first.payload, 4);

    for (int i = 0; i < 5; i++) {
      NetControlPacket repeat;
      Assert::That(peer.Send(kNetControlConnect, peer.clientNonce, 4),
                   Equals(true));
      Assert::That(peer.Await(server, kNetControlConnectAccept, repeat),
                   Equals(true));
      Assert::That(std::memcmp(repeat.payload, issued, 4), Equals(0));
    }

    // And the handshake still completes against that one nonce.
    uint8_t ready[8];
    std::memcpy(ready, peer.clientNonce, 4);
    std::memcpy(ready + 4, issued, 4);
    Assert::That(peer.Send(kNetControlConnectReady, ready, 8), Equals(true));
    NetControlPacket accept;
    Assert::That(peer.Await(server, kNetControlAccept, accept), Equals(true));
    Assert::That(std::memcmp(accept.payload, issued, 4), Equals(0));
    Assert::That(server.GetClientCount(), Equals(1));
  };

  It(should_issue_a_different_nonce_to_every_peer) {
    NetServer server;
    Assert::That(server.Start(0, 4), Equals(true));

    std::set<uint32_t> nonces;
    for (uint8_t tag = 1; tag <= 4; tag++) {
      RawPeer peer(server, tag);
      NetControlPacket accepted;
      Assert::That(peer.Send(kNetControlConnect, peer.clientNonce, 4),
                   Equals(true));
      Assert::That(peer.Await(server, kNetControlConnectAccept, accepted),
                   Equals(true));
      nonces.insert(NonceToToken(accepted.payload));
    }
    Assert::That((int)nonces.size(), Equals(4));
  };

  It(should_clamp_an_over_long_control_payload_on_the_wire) {
    // The shared NetSendControl is the single place the payload bound lives
    // now that NetServer and NetClient both forward to it (P38); an unclamped
    // memcpy here is what smashed the stack in P3.
    NetSocket receiver;
    Assert::That(receiver.Open(0), Equals(true));
    NetSocket sender;
    Assert::That(sender.Open(0), Equals(true));
    NetAddress to = NetAddressFromParts(0x7F000001u, receiver.GetBoundPort());

    std::string huge(kNetMaxPayload + 5000, 'x');
    Assert::That(NetSendControl(sender, to, kNetControlClose, huge.data(),
                                (int)huge.size()),
                 Equals(true));

    uint8_t buf[kNetMaxPacketSize];
    NetAddress from;
    int n = -1;
    uint32_t start = NetNowMs();
    while (n < 0 && NetNowMs() - start < (uint32_t)kPumpDeadlineMs) {
      n = receiver.Recv(from, buf, sizeof(buf));
      if (n < 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(kPumpStepMs));
    }
    Assert::That(n, Equals(kNetMaxPayload + 2));
    NetControlPacket ctrl;
    Assert::That(ctrl.Unpack(buf, n), Equals(true));
    Assert::That(ctrl.message, Equals((int)kNetControlClose));
    Assert::That(ctrl.payloadSize, Equals(kNetMaxPayload));
    Assert::That(ctrl.payload[kNetMaxPayload - 1], Equals((uint8_t)'x'));
  };

  It(should_refuse_a_negative_control_payload_size) {
    NetSocket sender;
    Assert::That(sender.Open(0), Equals(true));
    NetAddress to = NetAddressFromParts(0x7F000001u, 1);
    uint8_t payload[4] = {};
    Assert::That(NetSendControl(sender, to, kNetControlConnect, payload, -1),
                 Equals(false));
  };
};
