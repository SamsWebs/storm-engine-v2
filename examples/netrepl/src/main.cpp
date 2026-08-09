#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unistd.h>

#include <stormengine2/net/net.h>

// ── netrepl: fixed-tick host simulation + snapshot replication ──────────────
//   ./bin/netrepl host [port]
//   ./bin/netrepl join <ip> <port>
//
// The host simulates players bouncing around a field at 60 Hz and sends each
// client the varint-packed delta between that client's confirmed snapshot and
// the current tick. Clients apply deltas against their base snapshot, store
// the result in the cache, and render the field. Run until Ctrl-C.

namespace {
constexpr int kMsgDelta = 2; // [type][tick][delta bytes]
constexpr int kTickHz = 60;
constexpr int kTickMs = 1000 / kTickHz;
constexpr int kFieldW = 40;
constexpr int kFieldH = 12;
constexpr int kNumPlayers = 6;

struct Player {
  int32_t x, y, vx, vy;
};

void StepPlayers(Player *players, int n) {
  for (int i = 0; i < n; i++) {
    Player &p = players[i];
    p.x += p.vx;
    p.y += p.vy;
    if (p.x < 0) {
      p.x = 0;
      p.vx = -p.vx;
    }
    if (p.x >= kFieldW) {
      p.x = kFieldW - 1;
      p.vx = -p.vx;
    }
    if (p.y < 0) {
      p.y = 0;
      p.vy = -p.vy;
    }
    if (p.y >= kFieldH) {
      p.y = kFieldH - 1;
      p.vy = -p.vy;
    }
  }
}

// One delta message: [type][tick][varint-packed delta from the base]
int RunHost(uint16_t port) {
  Player players[kNumPlayers];
  for (int i = 0; i < kNumPlayers; i++) {
    players[i].x = 4 + (i * 6) % (kFieldW - 8);
    players[i].y = 1 + (i * 3) % (kFieldH - 2);
    players[i].vx = (i % 2 ? 1 : -1) * (1 + i % 3);
    players[i].vy = ((i / 2) % 2 ? 1 : -1) * (1 + i % 2);
  }

  // NetServer is ~369 KB (16 connection slots inline), so it does not belong
  // on the stack. A desktop main() has 8 MB and hides this, but SDL runs the
  // game on its own thread on Android, where the stack is far smaller.
  // Per-client confirmed snapshots; an empty finished snapshot means the
  // first delta a client gets is the full world state. Declared ahead of the
  // server so the connect callback can reset a slot's base.
  NetSnapshot bases[NetServer::kMaxClients];
  for (NetSnapshot &base : bases)
    base.Finish();

  auto serverOwner = std::make_unique<NetServer>();
  NetServer &server = *serverOwner;
  server.SetOnClientConnect([&](int clientId) {
    printf("== client %d joined ==\n", clientId);
    // Slot ids are recycled, so this one may still hold the departed
    // client's confirmed snapshot. The new client's own base is empty, so
    // encoding against the old one sends per-tick diffs that it decodes as
    // absolute positions — every player draws bunched in the corner, and
    // because the bases stay diverged it never recovers.
    bases[clientId] = NetSnapshot();
    bases[clientId].Finish();
  });
  server.SetOnClientDisconnect([&](int clientId, const std::string &reason) {
    printf("== client %d left: %s ==\n", clientId, reason.c_str());
  });
  if (!server.Start(port, 8)) {
    fprintf(stderr, "netrepl: cannot bind port %u\n", port);
    return 1;
  }

  printf("replication host on port %u — %d players at %d Hz (Ctrl-C to stop)\n",
         server.GetPort(), kNumPlayers, kTickHz);

  int tick = 0;
  for (;;) {
    uint32_t frameStart = NetNowMs();
    StepPlayers(players, kNumPlayers);

    NetSnapshot snap;
    for (int i = 0; i < kNumPlayers; i++)
      snap.AddItem(1, (uint16_t)i, &players[i].x, 2); // type, id, x/y
    snap.Finish();

    for (int i = 0; i < NetServer::kMaxClients; i++) {
      if (!server.IsClientConnected(i))
        continue;
      NetSnapshot &base = bases[i];
      // Fixed bound, not a VLA sized by EstimateSize: VLAs are a GCC extension
      // MSVC rejects, and the estimate bounds the delta rather than what a
      // NetMessageWriter can actually carry (kMaxSize == kNetMaxChunkSize).
      uint8_t delta[kNetMaxChunkSize];
      int n = NetSnapshotDelta::Create(base, snap, delta, sizeof(delta));
      if (n > 0) { // 0 = nothing changed, send nothing
        NetMessageWriter msg;
        msg.WriteInt(kMsgDelta);
        msg.WriteInt(tick);
        // Advance the base only once the delta has actually gone out. WriteRaw
        // fails when the message would exceed kMaxSize; advancing anyway would
        // leave the client's base behind the host's for good, and every later
        // delta would decode against the wrong world with nothing reporting it.
        if (!msg.WriteRaw(delta, n) ||
            !server.Send(i, msg.Data(), msg.Size(), true))
          continue;
        base = snap; // the client now has this as its base
      }
    }
    tick++;

    server.Update();
    server.Poll();

    int32_t elapsed = NetNowMs() - frameStart;
    if (elapsed < kTickMs)
      usleep((kTickMs - elapsed) * 1000);
  }
  return 0;
}

void RenderField(const NetSnapshot &snap, int tick) {
  char grid[kFieldH][kFieldW + 1];
  for (int y = 0; y < kFieldH; y++) {
    std::memset(grid[y], '.', kFieldW);
    grid[y][kFieldW] = '\0';
  }
  for (int i = 0; i < snap.NumItems(); i++) {
    uint16_t type = 0, id = 0;
    const int32_t *d = nullptr;
    int count = 0;
    if (!snap.GetItemByIndex(i, type, id, d, count) || count < 2)
      continue;
    if (d[0] >= 0 && d[0] < kFieldW && d[1] >= 0 && d[1] < kFieldH)
      grid[d[1]][d[0]] = (char)('0' + id % 10);
  }
  printf("tick %d · %d players\n", tick, snap.NumItems());
  for (int y = 0; y < kFieldH; y++)
    printf("|%s|\n", grid[y]);
}

int RunClient(const std::string &ip, uint16_t port) {
  // ~200 KB — off the stack, same reasoning as the host path above.
  auto clientOwner = std::make_unique<NetClient>();
  NetClient &client = *clientOwner;
  client.SetOnConnect([&]() {
    printf("== connected to %s:%u — replicating ==\n", ip.c_str(), port);
  });
  client.SetOnDisconnect([&](const std::string &reason) {
    printf("== disconnected: %s ==\n", reason.c_str());
  });

  NetSnapshot base; // starts empty: the first delta is the full world
  base.Finish();
  client.SetOnChunk([&](const NetChunk &chunk) {
    NetMessageReader r(chunk.data, chunk.size);
    int32_t type = 0, tick = 0;
    if (!r.ReadInt(type) || type != kMsgDelta)
      return;
    if (!r.ReadInt(tick))
      return;
    NetSnapshot rebuilt;
    if (!NetSnapshotDelta::Apply(base, chunk.data + r.Position(),
                                 chunk.size - r.Position(), rebuilt))
      return;
    base = rebuilt;
    client.StoreSnapshot(tick, rebuilt);
  });

  if (!client.Connect(ip, port)) {
    fprintf(stderr, "netrepl: cannot reach %s:%u\n", ip.c_str(), port);
    return 1;
  }

  int lastRendered = -1;
  for (;;) {
    client.Update();
    client.Poll();
    int latest = client.GetLatestSnapshotTick();
    if (latest >= 0 && latest - lastRendered >= 15) { // ~4 renders/second
      if (const NetSnapshot *snap = client.GetSnapshot(latest))
        RenderField(*snap, latest);
      lastRendered = latest;
    }
    usleep(2000);
  }
  return 0;
}
} // namespace

int main(int argc, char *argv[]) {
  if (argc >= 2 && std::string(argv[1]) == "host")
    return RunHost(argc >= 3 ? (uint16_t)atoi(argv[2]) : 5000);
  if (argc >= 3 && std::string(argv[1]) == "join")
    return RunClient(argv[2], argc >= 4 ? (uint16_t)atoi(argv[3]) : 5000);
  printf("usage:\n  netrepl host [port]\n  netrepl join <ip> <port>\n");
  return 1;
}
