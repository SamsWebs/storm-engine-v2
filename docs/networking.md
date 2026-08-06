# Networking

UDP transport and replication for hosting and joining games. The design follows
Teeworlds 0.7.5 (zlib license): a cookie handshake to defeat address spoofing,
reliability *on demand* (only what must arrive does), an authoritative host
that simulates the world at a fixed tick rate, and varint-packed snapshot
deltas so the wire only carries what changed.

## Layer map

| Class | Role |
|---|---|
| `NetSocket` | Non-blocking UDP socket (BSD sockets, winsock on Windows); the only OS-touching layer |
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
#include <memory>
#include <stormengine2/net/net.h>

auto server = std::make_unique<NetServer>();   // ~369 KB — do not stack it
server->SetOnClientConnect([&](int clientId) {
    // send the joining client the initial game state
});
server->SetOnChunk([&](int clientId, const NetChunk &chunk) {
    // parse one message with NetMessageReader (chunk.data is borrowed —
    // see "Chunk lifetime" below)
});

if (!server->Start(0, 8))         // port 0 = ephemeral; read server->GetPort()
    return;

while (running) {
    server->Update();             // retries, keepalives, timeouts, bans
    server->Poll();               // receive + dispatch, flush unreliable sends
    // simulate the world, then replicate. Client ids are slot indices, not a
    // dense 0..count-1 range, so walk the slot table and test each slot:
    for (int id = 0; id < NetServer::kMaxClients; id++) {
        if (!server->IsClientConnected(id))
            continue;
        server->Send(id, msg.Data(), msg.Size(), /*vital=*/true);
    }
}
```

`NetServer` is equally happy as a listen-on-LAN host or a headless dedicated
server process. `Start(port, maxClients)` with `maxClients` 1..16; a client
that joins when the server is full gets a `CLOSE "server full"` and never
occupies a slot.

### Client ids are slot indices, not a dense range

A client id is a permanent index into a fixed table of `NetServer::kMaxClients`
(16) slots. Disconnecting frees a slot **in place** — the table is never
compacted — and the next joiner takes the lowest free index, which may be a
hole in the middle. `GetClientCount()` returns how many slots are *occupied*;
it is a population, never a maximum id.

So do **not** write `for (int i = 0; i < server.GetClientCount(); i++)`. With
ids 0 and 1 connected, player 0 quitting drops the count to 1, and that loop
then sends only to id 0 — which is now free, so `Send` returns `false` — while
player 1 silently stops receiving anything for the rest of the match. It works
perfectly in a two-player test where nobody leaves, which is exactly what makes
it dangerous.

Two correct patterns, both on the public API:

```cpp
// Everyone gets the same bytes: one call, packed once per slot.
server->Broadcast(msg.Data(), msg.Size(), /*vital=*/true);

// Per-client payloads (snapshot deltas, private state): enumerate the slots.
for (int id = 0; id < NetServer::kMaxClients; id++) {
    if (!server->IsClientConnected(id))
        continue;
    server->Send(id, PayloadFor(id), SizeFor(id), true);
}
```

Iterate to `kMaxClients`, not to `maxClients` as passed to `Start` — the bound
is the table size, and `IsClientConnected` range-checks anyway. An id stays
valid for that client's whole session and is handed out again only after the
slot is freed, so it is safe to key per-client game state by id as long as the
disconnect callback clears it. `Send` and `Broadcast` return `false` when
nothing was queued (unknown/offline id, or `size > kNetMaxChunkSize`); check
the return on anything that must not vanish.

### Object sizes

`NetServer` and `NetClient` carry their buffers inline and are far too big for
a thread stack: `NetServer` ≈ 369 KB (16 slots × a 23 KB `NetConnection` with
its 16 KB resend pool), `NetClient` ≈ 200 KB (a 176 KB 16-tick snapshot
cache), `NetSnapshot` ≈ 11 KB. A desktop main thread's 8 MB absorbs that; a
Switch or Android game thread, or any SDL-created thread, does not. Heap
allocate them (`std::make_unique`) or make them members of an object that is
itself heap allocated.

They are also *implicitly copyable and must never be copied* — both install
send callbacks that capture `this`, and `NetSocket` owns a file descriptor its
destructor closes, so a copy sends through the original's socket and
double-closes the fd. The compiler will not stop you (see `KNOWN_ISSUES.md`
§6, frozen for 1.x); hold them by `unique_ptr` or reference, and never in a
`std::vector` that can reallocate.

## Joining a game

```cpp
auto client = std::make_unique<NetClient>();   // ~200 KB — do not stack it
client->SetOnConnect([&]() { /* start the match */ });
client->SetOnDisconnect([&](const std::string &reason) { /* back to menu */ });
client->SetOnChunk([&](const NetChunk &chunk) {
    // NetMessageReader over chunk.data / chunk.size
});

// The host's own LAN address, or "127.0.0.1" for two processes on one machine.
// Connect() only fails here on a bad address or a socket it cannot open — it
// does not wait for the server. Pointing at an address nothing is listening on
// succeeds, then times out through SetOnDisconnect about ten seconds later.
if (!client->Connect(hostAddress, 4321))
    return;

while (running) {
    client->Update();             // handshake retries, keepalives, timeouts
    client->Poll();               // receive + dispatch, flush unreliable sends
}
```

The client's socket is bound to an ephemeral port; stray traffic from other
addresses is ignored. `client.Disconnect(reason)` closes politely (the host
hears the reason), and the host's own `DisconnectClient(id, reason)` / the
peer's `CLOSE` message both surface through the disconnect callbacks.

### Update() before Poll(), every frame, on both sides

The order is a contract, not a style preference. `Update(now)` is the only
thing that hands the connection layer the current time — it is what stamps
`lastRecvMs`, RTT samples and resend timers taken during the `Poll()` that
follows. Call `Poll()` first and every timestamp that pass records is one
frame stale (and before the first `Update()`, it is whatever the clock read at
`Start`). Skipping `Update()` for several frames — a long level load, a
blocking asset decode — burns real milliseconds against `kNetTimeoutMs` with
no keepalives going out, and the peer drops you. Pump both calls from the same
loop and the same clock as the rest of the game.

### Chunk lifetime

`NetChunk::data` points into the receiving connection's scratch buffer and is
valid **only for the duration of the callback**. The next `Feed` — i.e. the
next datagram inside the very same `Poll()` — overwrites it. Parse it, or copy
what you need out of it, before returning; never stash the pointer, and never
hand it to something that outlives the callback (a queue, a lambda captured by
reference, a thread).

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
msg.WriteString(ownerName.c_str());   // writes length + bytes incl. terminator
server->Send(clientId, msg.Data(), msg.Size(), /*vital=*/true);

// receiving
NetMessageReader r(chunk.data, chunk.size);
int32_t type = 0;
if (!r.ReadInt(type) || type != kMsgFranchiseUpdated)
    return;
int32_t id = 0;
char name[64];
if (!r.ReadInt(id) || !r.ReadString(name, sizeof(name)))
    return;                           // truncated or hostile packet
```

**Check every read.** The reader is the game's trust boundary: `chunk.data` is
whatever a peer put on the wire. `ReadInt` fails on a truncated or non-minimal
varint; `ReadString` fails when the length is non-positive, runs past the
payload, exceeds `outSize`, or the wire bytes carry no terminator of their own.
It always null-terminates `out` (including on every failure path, so a
discarded return leaves an empty string rather than uninitialised stack), and
it deliberately refuses rather than terminating at `outSize - 1`, because that
would hand you the leftover bytes of your own buffer as if the peer had sent
them. A read that fails leaves the reader positioned mid-message, so abandon
the whole message on the first `false` — do not keep reading fields.

`NetMessageWriter` caps at `kNetMaxChunkSize` (1200 bytes) and every write
returns `false` once full; `Size()` is the exact byte count to pass to `Send`.

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

uint8_t delta[kNetMaxChunkSize];                        // fixed 1200 B, no VLA
int n = NetSnapshotDelta::Create(prevSnap, tickSnap, delta, sizeof(delta));
if (n < 0)
    return;                       // did not fit — keep the old base, see below
if (n > 0 && !server->Send(clientId, delta, n, true))
    return;                       // not queued — keep the old base
prevSnap = tickSnap;              // advance only once the delta is on its way
```

`Create` returns the byte count written, `0` when nothing changed (send
nothing), and `-1` when the delta does not fit in `dst`.

**Size the buffer with a constant, not `EstimateSize`.** Earlier revisions of
this page showed `uint8_t delta[NetSnapshotDelta::EstimateSize(a, b)]`. That is
a C99 variable-length array — a GCC/Clang extension that standard C++ does not
have and MSVC rejects outright, so it will not build the moment the game is
ported to the Windows or Switch toolchains. It is also the wrong bound in both
directions: at the `kMaxItems` / `kMaxDataInts` ceilings `EstimateSize` can
return 15,874 bytes, which is 13 KB of stack per tick per client, and yet a
delta that big can never be sent — `Send` refuses any chunk over
`kNetMaxChunkSize` (1200 bytes). `kNetMaxChunkSize` is therefore the only
buffer size worth allocating: anything `Create` cannot fit in it was
unsendable anyway. `EstimateSize` remains useful as a cheap "will this fit?"
predicate before building the delta.

If a tick's delta genuinely exceeds 1200 bytes, split the state across several
snapshots (one per item type, say) and send one delta per snapshot with its own
per-client base — do not grow the buffer. **Never advance the base snapshot on
a delta you did not send**: the client's base and the host's would diverge, and
every later delta would decode into a wrong world.

```cpp
// client, on a snapshot chunk:
NetSnapshot rebuilt;
if (NetSnapshotDelta::Apply(base, chunk.data, chunk.size, rebuilt)) {
    client->StoreSnapshot(tick, rebuilt);  // next delta's base + prediction
    base = rebuilt;
}
```

**`Apply`'s `deltaSize` must be exactly what `Create` returned.** Trailing
bytes are rejected — passing `sizeof(buf)` for an over-allocated buffer fails,
even though the leading bytes parse perfectly. `chunk.size` is exactly right
here, because the chunk carries only the bytes that were sent. If you stage a
delta somewhere else, carry its length alongside it. `Apply` also requires the
same `from` snapshot the delta was created against; on any `false` return
discard the result and keep the previous base (the target snapshot is left
partially rebuilt).

Clients query `client.GetSnapshot(tick)` (a 16-tick cache) to rewind or
predict against the latest authoritative state; `GetLatestSnapshotTick()` gives
the freshest one. `NetSnapshot::Crc()` is a cheap stable checksum for debug
comparisons. Keys are `(type << 16) | id`, so a snapshot holds e.g. skaters
(type 1, id 0..11) alongside the puck (type 2, id 0) without collisions. The
full `uint16_t` range is usable for both halves — a type with the top bit set
rides the wire as a negative varint and round-trips byte-for-byte.

One snapshot holds at most `NetSnapshot::kMaxItems` (256) items and
`kMaxDataInts` (2048) int32s in total. `AddItem` returns `false` when either
ceiling is hit, when called after `Finish()`, and when it would replace an
existing key with a *different* int count — replacement in place requires the
same count, so `Reset()` and rebuild if an entity's payload width changes.
Check the return: a dropped item is a silently missing entity on the client.

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

Chunk sizes are 12 bits on the wire; the sender caps them at `kNetMaxChunkSize`
(1200) so a chunk plus its header always fits one datagram — `Send`,
`Broadcast` and `NetMessageWriter` all refuse anything larger. Vital sequences
are 10 bits and wrap at 1024. Control datagrams (before any connection exists)
start with the magic byte `0xCF` — impossible to collide with a connected
header:

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

Because the nonce *is* the token, and the token travels in cleartext in every
connected packet header, two properties of the generator matter to games:

- **Nonces are drawn from a ChaCha20 keystream** keyed once per process from
  `std::random_device` (the OS entropy source), mixed with the clock and the
  process's own layout. Observing any number of issued nonces reveals nothing
  about the next one. The earlier generator was a raw xorshift64, whose whole
  state six observed nonces recovered — do not build a security argument on
  nonces from an engine older than 1.2.2.
- **A slot's server nonce rotates on every `CONNECT` except a genuine
  mid-handshake retry.** Re-sending `CONNECT` while still at step 1 gets the
  same nonce back, so the client's own 500 ms retries do not churn it; a
  `CONNECT` aimed at a slot that is already online forces a fresh one, because
  reusing it would let anyone who captured one datagram replay the token
  through a spoofed `CONNECT` / `CONNECT_READY` and seize the slot. The
  practical consequence for a game: a reconnecting player's token is never the
  token it held before, so never cache or key anything off a token across
  sessions.

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
| `kNetResendMaxEntries` | 96 | unacked vital chunks in flight |
| `kNetResendBufferSize` | 16384 | bytes of unacked vital payload in flight |
| `kNetMaxChunkSize` | 1200 | largest single `Send` / `Broadcast` payload |
| `kNetMaxPacketSize` | 1400 | datagram cap (7-byte header + chunks) |

### Overflowing the vital window drops the connection

The two resend limits are independent, and a vital send that cannot be
buffered under **either** one does not block, queue, or fail quietly — it puts
the connection into the error state with `"too weak connection (out of
buffer)"`, which `Update()` then reports as a disconnect on the very next
frame. `Send` returns `false` on that call, and every later `Send` on that
connection returns `false` too.

You can hit it two ways:

- **96 unacked chunks** (`kNetResendMaxEntries`), independent of size — the
  97th four-byte vital message with no ack in between is enough.
- **16 KB of unacked payload** (`kNetResendBufferSize`), or a chunk that would
  wrap the ring onto bytes a live unacked entry still owns.

A third path reaches the same place: `kNetHardResendMs`. If the oldest unacked
chunk is still unacked 10 s later, the connection errors with `"too weak
connection"` no matter how much room is left.

So sustained reliable throughput requires acks to keep up. Acks ride on the
receiver's own traffic, which means a peer that only sends keepalives acks at
roughly 1 Hz — burst more than 96 vital chunks (or 16 KB) into that window and
you kick your own player. Rate-limit bulk vital traffic against `GetRTT()`,
prefer non-vital for anything a later value supersedes, and pump
`Update()`/`Poll()` every frame on both sides so acks keep flowing.

## Operational notes

- IPv4 only for now; join by dotted IP or hostname (`getaddrinfo`, first IPv4
  result).
- `NetServer::Start(0, ...)` binds an OS-assigned port — read it back with
  `GetPort()` and print it for LAN play. `Start` also resets the slot table
  and clears every ban, so restarting a host forgets who was kicked.
- Per-IP cap (`kNetMaxClientsPerIp = 4`) plus the step-1 flood ban keep a
  single machine from exhausting the slot table. The cap counts slots in
  handshake as well as online ones.
- `BanIp(ipHost, seconds)` is instant and lasts for that server run or until it
  expires, whichever comes first — expiry is swept in `Update()`, so a host
  that stops pumping never expires a ban. `ipHost` is **host** byte order:
  pass `NetIpToHost(server->GetClientAddress(id))`, not the raw
  `NetAddress::ip`.
- The engine clock (`NetNowMs()`) is a steady millisecond timer; drive every
  `Update()` from the same clock source as the rest of the game loop.
- The socket layer is BSD sockets on Linux, macOS, Android (NDK) and Switch
  (libnx), and winsock behind `#ifdef` on Windows. Everything above it is pure
  logic and is spec'd without a socket.
