# Networking

UDP transport and replication for hosting and joining games. The design follows
Teeworlds 0.7.5 (zlib license): a cookie handshake to defeat address spoofing,
reliability *on demand* (only what must arrive does), an authoritative host
that simulates the world at a fixed tick rate, and varint-packed snapshot
deltas so the wire only carries what changed.

## Layer map

| Class | Role |
|---|---|
| `NetSocket` | Non-blocking POSIX UDP socket; the only OS-touching layer |
| `NetConnection` | Reliable transport between two peers: vital/non-vital chunks, 10-bit seq/ack window, resends, RTT, keepalives, timeouts |
| `NetServer` | Host: 16 fixed slots, cookie handshake, per-IP caps, bans |
| `NetClient` | Joiner: resolve + handshake + snapshot cache |
| `NetSnapshot` / `NetSnapshotDelta` / `NetSnapshotCache` | Tick state replication |
| `NetMessageWriter` / `NetMessageReader` | Varint message packing for game-defined messages |
| `NetVarInt` | Variable-length integer encoding used everywhere on the wire |

Everything except `NetSocket` is pure logic: the whole transport and protocol
is exercised by the specs in `specs/net/` without sockets, and the loopback
specs drive a real server and client end to end.

## Hosting a game

```cpp
#include <stormengine2/net/net.h>

NetServer server;
server.SetOnClientConnect([&](int clientId) {
    // send the joining client the initial game state
});
server.SetOnChunk([&](int clientId, const NetChunk &chunk) {
    // parse one message with NetMessageReader
});

if (!server.Start(0, 8))          // port 0 = ephemeral; read server.GetPort()
    return;

while (running) {
    server.Update();              // retries, keepalives, timeouts, bans
    server.Poll();                // receive + dispatch, flush unreliable sends
    // simulate the world, then replicate:
    for (int i = 0; i < server.GetClientCount(); i++)
        server.Send(i, msg.Data(), msg.Size(), /*vital=*/true);
}
```

`NetServer` is equally happy as a listen-on-LAN host or a headless dedicated
server process. `Start(port, maxClients)` with `maxClients` 1..16; a client
that joins when the server is full gets a `CLOSE "server full"` and never
occupies a slot. `Broadcast` fans one message to every connected client.

## Joining a game

```cpp
NetClient client;
client.SetOnConnect([&]() { /* start the match */ });
client.SetOnDisconnect([&](const std::string &reason) { /* back to menu */ });
client.SetOnChunk([&](const NetChunk &chunk) {
    // NetMessageReader over chunk.data / chunk.size
});

if (!client.Connect("192.168.1.20", 4321))
    return;

while (running) {
    client.Update();              // handshake retries, keepalives, timeouts
    client.Poll();                // receive + dispatch, flush unreliable sends
}
```

The client's socket is bound to an ephemeral port; stray traffic from other
addresses is ignored. `client.Disconnect(reason)` closes politely (the host
hears the reason), and the host's own `DisconnectClient(id, reason)` / the
peer's `CLOSE` message both surface through the disconnect callbacks.

## Game messages (the franchise pattern)

Chunks are opaque; the game gives them meaning. Pack a message type id first,
then fields — the type id is what makes a game's reliable messages (host
migrations, franchise persistence, chat, match setup) look identical to any
other message:

```cpp
// sending (either side)
NetMessageWriter msg;
msg.WriteInt(kMsgFranchiseUpdated);   // game-defined ids start at 1
msg.WriteInt(franchiseId);
msg.WriteString(ownerName.c_str());
server.Send(clientId, msg.Data(), msg.Size(), /*vital=*/true);

// receiving
int32_t type;
NetMessageReader r(chunk.data, chunk.size);
if (!r.ReadInt(type) || type != kMsgFranchiseUpdated)
    return;
int32_t id; char name[64];
r.ReadInt(id); r.ReadString(name, sizeof(name));
```

Reliable messages (`vital = true`) are flushed immediately and will arrive
lossless and in order — use them for anything that must not vanish. Unreliable
messages are cheaper and may be dropped — use them for position pings,
spectator updates, or any state superseded by newer values.

## Replication (snapshots)

The host simulates the world at a fixed tick. It keeps the base snapshot each
client has confirmed and sends the delta between that base and the current
tick (the game manages per-client bases — the code below shows one):

```cpp
// host, once per tick per client:
NetSnapshot tickSnap;
for (auto &skater : skaters)
    tickSnap.AddItem(1, skater.id, skater.pack3i, 3);   // type, id, int32s
tickSnap.Finish();

uint8_t delta[NetSnapshotDelta::EstimateSize(prevSnap, tickSnap)];
int n = NetSnapshotDelta::Create(prevSnap, tickSnap, delta, sizeof(delta));
if (n > 0)
    server.Send(clientId, delta, n, true);              // 0 = nothing changed
prevSnap = tickSnap;
```

(A `Create` of 0 bytes means nothing changed — send nothing.)

```cpp
// client, on a snapshot chunk:
NetSnapshot rebuilt;
if (NetSnapshotDelta::Apply(base, chunk.data, chunk.size, rebuilt)) {
    client.StoreSnapshot(tick, rebuilt);   // base for the next delta + prediction
    base = rebuilt;
}
```

Clients query `client.GetSnapshot(tick)` (a 16-tick cache) to rewind or
predict against the latest authoritative state; `GetLatestSnapshotTick()` gives
the freshest one. `NetSnapshot::Crc()` is a cheap stable checksum for debug
comparisons. Keys are `(type << 16) | id`, so a snapshot holds e.g. skaters
(type 1, id 0..11) alongside the puck (type 2, id 0) without collisions.

## Wire format

Connected packets (post-handshake), 1400-byte MTU cap so nothing fragments:

```
byte 0     flags (6 bits, NetPacketFlag) << 2 | ack high (2 bits)
byte 1     ack low (8 bits)                        <- 10-bit ack window
byte 2     numChunks
byte 3-6   token (the nonce the peer issued us)
bytes 7+   chunks:
  plain    [flags(2 bits) << 6 | sizeHigh(6 bits)] [sizeLow(6 bits)]     2 bytes
  vital    [flags(2 bits) << 6 | sizeHigh(6 bits)] [sizeLow(6 bits) |
           seqHigh(2 bits) << 6] [seqLow(8 bits)]                         3 bytes
```

Chunk sizes are 12 bits (max 1200), vital sequences are 10 bits and wrap at
1024. Control datagrams (before any connection exists) start with the magic
byte `0xCF` — impossible to collide with a connected header:

| Message | Direction | Payload | Meaning |
|---|---|---|---|
| `CONNECT` | client → server | client nonce (4B) | "I want to join" |
| `CONNECT_ACCEPT` | server → client | server nonce (4B) | cookie to echo |
| `CONNECT_READY` | client → server | client + server nonce (8B) | proves the client's address |
| `ACCEPT` | server → client | server nonce (4B) | slot granted; detects restarts |
| `CLOSE` | either | reason string | polite teardown |

Each step is re-sent every 500 ms until the protocol advances; a handshake
that stalls at `CONNECT` for 10 s (i.e. a spoofed address flood) earns that IP
a 60-second ban. The nonces double as per-connection header tokens: a spoofed
source can never know the cookie, so it cannot forge a single valid packet.

## Reliability

- **Vital chunks** carry a sequence number, are buffered until acked, and are
  retransmitted after 1 s. The receiver asks explicitly for resends with the
  `RESEND` packet flag when it detects a gap; a duplicate arriving within the
  512-entry "backroom" window (half the 10-bit sequence space) is dropped.
- **Non-vital chunks** are fire-and-forget: never retransmitted, possibly
  delivered out of order.
- Acks ride on the receiver's own traffic (its chunks, flushes, or keepalives)
  — nothing is ever sent just to ack, keeping idle connections quiet.
- RTT is estimated from acked chunks (exponentially smoothed).

| Constant | Value | Meaning |
|---|---|---|
| `kNetResendMs` | 1000 | vital chunk retransmit |
| `kNetHardResendMs` | 10000 | oldest unacked too long → "too weak connection" |
| `kNetFlushMs` | 500 | auto-flush queued chunks |
| `kNetKeepaliveMs` | 1000 | idle keepalive |
| `kNetTimeoutMs` | 10000 | no traffic at all → timeout |
| `kNetHandshakeRetryMs` | 500 | handshake step resend |
| `kNetResendMaxEntries` | 96 | unacked vital chunk window (≈16 KB) |

Sustained reliable throughput therefore requires acks to keep up: a receiver
that never sends anything but keepalives caps the sender at ~96 in-flight
chunks, and a game loop that pumps `Update()`/`Poll()` every frame (and sends
input up) keeps the window flowing.

## Operational notes

- IPv4 only for now; join by dotted IP or hostname (`getaddrinfo`, first IPv4
  result).
- `NetServer::Start(0, ...)` binds an OS-assigned port — read it back with
  `GetPort()` and print it for LAN play.
- Per-IP cap (`kNetMaxClientsPerIp = 4`) plus the step-1 flood ban keep a
  single machine from exhausting the slot table.
- `BanIp(ipHost, seconds)` is instant and persisted per server run.
- The engine clock (`NetNowMs()`) is a steady millisecond timer; drive every
  `Update()` from the same clock source as the rest of the game loop.
