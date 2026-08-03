#include <cstring>
#include <vector>

#include <igloo/igloo_alt.h>

#include "../../common/net/netConnection.h"

using namespace igloo;

namespace {
const uint32_t kTokenA = 0x1111AAAA;
const uint32_t kTokenB = 0x2222BBBB;

struct Pipe {
  std::vector<std::vector<uint8_t>> packets;
  void Push(const uint8_t *data, int size) {
    packets.emplace_back(data, data + size);
  }
};

// Two connections over an in-memory wire. Each Step() advances the clock,
// runs both Update()s, delivers every pending packet in each direction
// (like a kernel receive queue), and drains the recipient's pending chunks
// (int32 payloads go to bReceived).
struct Pair {
  NetConnection a, b;
  Pipe aToB, bToA;
  uint32_t now = 1000;
  std::vector<int32_t> bReceived;

  Pair() {
    a.SetSendFunc([this](const uint8_t *d, int s) {
      aToB.Push(d, s);
      return true;
    });
    b.SetSendFunc([this](const uint8_t *d, int s) {
      bToA.Push(d, s);
      return true;
    });
    a.Start(now, kTokenA, kTokenB); // A verifies kTokenA, stamps kTokenB
    b.Start(now, kTokenB, kTokenA); // B verifies kTokenB, stamps kTokenA
  }

  void DrainB() {
    NetChunk chunk;
    while (b.NextChunk(chunk)) {
      if (chunk.size == (int)sizeof(int32_t)) {
        int32_t v = 0;
        std::memcpy(&v, chunk.data, sizeof(v));
        bReceived.push_back(v);
      }
    }
  }

  void SendInt(NetConnection &c, bool vital, int32_t value) {
    now += 1;
    c.Update(now);
    Assert::That(c.QueueChunk(vital, &value, sizeof(value)), Equals(true));
    c.Flush(false);
  }

  void DeliverAll(Pipe &pipe, NetConnection &to) {
    while (!pipe.packets.empty()) {
      auto &pkt = pipe.packets.front();
      to.Feed(pkt.data(), (int)pkt.size());
      pipe.packets.erase(pipe.packets.begin());
      if (&to == &b)
        DrainB();
    }
  }

  // Advances time and exchanges one packet each way. Returns the number
  // of chunks delivered to B.
  int Step() {
    now += 10;
    a.Update(now);
    b.Update(now);
    int before = (int)bReceived.size();
    DeliverAll(aToB, b);
    DeliverAll(bToA, a);
    return (int)bReceived.size() - before;
  }

  int Run(int steps) {
    int total = 0;
    for (int i = 0; i < steps; i++)
      total += Step();
    return total;
  }
};
} // namespace

Describe(NetConnectionSpec) {

  Describe(ReliableDelivery) {
    It(should_deliver_vital_chunks_in_order) {
      Pair p;
      for (int i = 1; i <= 10; i++)
        p.SendInt(p.a, true, i);
      Assert::That(p.Run(30), Equals(10));
      Assert::That(p.bReceived, Equals(std::vector<int32_t>(
                                    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10})));
    };
    It(should_retransmit_a_dropped_packet) {
      Pair p;
      p.SendInt(p.a, true, 1);
      p.SendInt(p.a, true, 2);
      p.aToB.packets.clear();              // both dropped on the wire
      Assert::That(p.Run(300), Equals(2)); // > 1s: A retransmits
    };
    It(should_resend_on_peer_resend_request) {
      Pair p;
      p.SendInt(p.a, true, 1);
      p.SendInt(p.a, true, 2);
      p.SendInt(p.a, true, 3);
      // Deliver 3 first: B sees a gap (expects 1) and asks for a resend.
      std::vector<uint8_t> third = p.aToB.packets[2];
      p.aToB.packets.clear();
      p.b.Feed(third.data(), (int)third.size());
      Assert::That(p.Run(300), Equals(3)); // 1, 2 resent; 3 delivered once
    };
    It(should_deduplicate_duplicate_packets) {
      Pair p;
      p.SendInt(p.a, true, 7);
      std::vector<uint8_t> dup = p.aToB.packets[0];
      p.aToB.packets.clear();
      p.b.Feed(dup.data(), (int)dup.size());
      p.DrainB();
      Assert::That(p.bReceived, Equals(std::vector<int32_t>({7})));
      p.b.Feed(dup.data(), (int)dup.size()); // replay the datagram
      Assert::That(p.Run(5), Equals(0));
    };
    It(should_handle_sequence_wraparound) {
      Pair p;
      int total = 0;
      for (int i = 1; i <= 1050; i++) {
        p.SendInt(p.a, true, i);
        if (i % 30 == 0) {
          total += p.Run(2); // deliver the batch to B
          p.b.Flush(true);   // B acks on its own send cadence
          total += p.Run(1); // the ack frees A's resend buffer
        }
      }
      total += p.Run(20);
      Assert::That(total, Equals(1050));
    };
    It(should_estimate_rtt_after_acks) {
      Pair p;
      p.SendInt(p.a, true, 1);
      p.now += 200;
      p.b.Update(p.now);
      p.DeliverAll(p.aToB, p.b);
      p.b.Flush(true); // ack
      p.a.Update(p.now);
      p.DeliverAll(p.bToA, p.a);
      Assert::That(p.a.GetRTT(), Is().GreaterThanOrEqualTo(199));
    };
    It(should_retransmit_identical_bytes_past_the_resend_ring_wrap) {
      // Pushing more than kNetResendBufferSize of unacked vital payload
      // forces the resend ring to wrap. A wrap must only land in bytes
      // that are already acked (dead); wrapping into a live entry makes
      // the retransmit read back the wrong bytes (P2). This drives the
      // write pointer to e.g. 16500 -> overflow, which wraps the tail
      // chunks below the read frontier, then retransmits everything.
      Pair p;
      const int kChunkSize = 300;
      const int kTotal = 64; // 10 acked + 54 live: exactly 16 KB live
      std::vector<std::vector<uint8_t>> sent;

      // Chunks 0..9: delivered and acked, freeing [0, 3000) as legal
      // wrap space below the read frontier.
      for (int i = 0; i < 10; i++) {
        std::vector<uint8_t> blob(kChunkSize);
        for (int j = 0; j < kChunkSize; j++)
          blob[j] = (uint8_t)(i * kChunkSize + j);
        sent.push_back(blob);
        Assert::That(p.a.QueueChunk(true, blob.data(), kChunkSize),
                     Equals(true));
        p.a.Flush(false);
        p.DeliverAll(p.aToB, p.b);
        p.b.Flush(true); // ack
        p.DeliverAll(p.bToA, p.a);
      }

      // Chunks 10..39: queued but never delivered; the write pointer
      // crosses the buffer end near chunk 54 so chunks 54..63 wrap down
      // into the acked [0, 3000) region and chunks 64+ are refused
      // (overlap), which is the correct capacity behaviour.
      for (int i = 10; i < kTotal; i++) {
        std::vector<uint8_t> blob(kChunkSize);
        for (int j = 0; j < kChunkSize; j++)
          blob[j] = (uint8_t)(i * kChunkSize + j);
        sent.push_back(blob);
        Assert::That(p.a.QueueChunk(true, blob.data(), kChunkSize),
                     Equals(true));
        p.a.Flush(false);
      }
      p.aToB.packets.clear(); // drop everything still in flight

      // B asks A to resend everything it holds (RESEND packet flag).
      uint8_t req[7];
      NetPacketHeaderPack(req, kNetPacketResend, 0, 0, kTokenA);
      Assert::That(p.a.Feed(req, 7), Equals(0));
      p.a.Flush(true); // flush the final partial batch

      // Deliver the retransmits and capture every received chunk.
      std::vector<std::vector<uint8_t>> received;
      while (!p.aToB.packets.empty()) {
        std::vector<uint8_t> pkt = p.aToB.packets.front();
        p.aToB.packets.erase(p.aToB.packets.begin());
        Assert::That(p.b.Feed(pkt.data(), (int)pkt.size()), Equals(1));
        NetChunk chunk;
        while (p.b.NextChunk(chunk))
          received.emplace_back(chunk.data, chunk.data + chunk.size);
      }

      // B must have received exactly the chunks it never saw, byte for
      // byte — in sequence order, since all vital chunks are ordered.
      const int kStart = 10;
      Assert::That((int)received.size(), Equals(kTotal - kStart));
      for (int i = kStart; i < kTotal; i++)
        Assert::That(received[i - kStart] == sent[i], Equals(true));
    };
  };

  Describe(NonVital) {
    It(should_not_retransmit_non_vital_chunks) {
      Pair p;
      p.SendInt(p.a, false, 1);
      p.SendInt(p.a, false, 2);
      p.aToB.packets.clear(); // dropped
      Assert::That(p.Run(300), Equals(0));
    };
    It(should_deliver_non_vital_chunks_when_the_packet_arrives) {
      Pair p;
      p.SendInt(p.a, false, 42);
      Assert::That(p.Run(5), Equals(1));
      Assert::That(p.bReceived, Equals(std::vector<int32_t>({42})));
    };
  };

  Describe(Auth) {
    It(should_reject_packets_with_the_wrong_token) {
      Pair p;
      p.SendInt(p.a, true, 1);
      std::vector<uint8_t> forged = p.aToB.packets[0];
      forged[3] ^= 0xFF; // token bytes
      forged[4] ^= 0xFF;
      forged[5] ^= 0xFF;
      forged[6] ^= 0xFF;
      Assert::That(p.b.Feed(forged.data(), (int)forged.size()), Equals(-1));
    };
    It(should_reject_out_of_range_acks) {
      Pair p;
      p.SendInt(p.a, true, 1);
      p.SendInt(p.a, true, 2);
      p.Run(10); // B acked both
      Assert::That(p.aToB.packets.empty(), Equals(true));
      p.SendInt(p.a, true, 3);
      std::vector<uint8_t> bogus = p.aToB.packets[0];
      bogus[0] |= 0x02; // ack = 0x200, far ahead of anything A has sent
      Assert::That(p.a.Feed(bogus.data(), (int)bogus.size()), Equals(-1));
    };
    It(should_reject_queueing_while_offline) {
      Pair p;
      p.a.Stop();
      int32_t v = 1;
      Assert::That(p.a.QueueChunk(true, &v, sizeof(v)), Equals(false));
      p.a.Update(p.now);
      Assert::That(p.a.GetState(), Equals(NetConnection::kOffline));
    };
  };

  Describe(Timeouts) {
    It(should_error_after_silence) {
      Pair p;
      p.now += kNetTimeoutMs + 100;
      p.a.Update(p.now);
      Assert::That(p.a.GetState(), Equals(NetConnection::kError));
      Assert::That(std::string(p.a.GetError()), Contains("timeout"));
    };
    It(should_error_when_a_vital_chunk_goes_unacked_too_long) {
      Pair p;
      p.SendInt(p.a, true, 1);
      p.aToB.packets.clear(); // never delivered
      p.now += kNetHardResendMs + 100;
      p.a.Update(p.now);
      Assert::That(p.a.GetState(), Equals(NetConnection::kError));
      Assert::That(std::string(p.a.GetError()), Contains("weak"));
    };
    It(should_send_keepalives_when_idle) {
      Pair p;
      p.Run(150); // 1.5s with no traffic
      Assert::That(p.a.GetState(), Equals(NetConnection::kOnline));
      Assert::That(p.b.GetState(), Equals(NetConnection::kOnline));
    };
    It(should_resume_after_both_peers_restart) {
      Pair p;
      p.SendInt(p.a, true, 1);
      p.Run(10);
      Assert::That(p.bReceived, Equals(std::vector<int32_t>({1})));
      // A reconnect means a fresh handshake on both ends.
      p.a.Stop();
      p.b.Stop();
      p.a.Start(p.now, kTokenA, kTokenB);
      p.b.Start(p.now, kTokenB, kTokenA);
      p.SendInt(p.a, true, 2);
      p.Run(10);
      Assert::That(p.bReceived, Equals(std::vector<int32_t>({1, 2})));
    };
  };
};
