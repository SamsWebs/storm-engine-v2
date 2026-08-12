# netchat

A console chat over UDP built on the engine's `net/` module - the smallest complete example of hosting a game and joining it. One process hosts on a port; any number of others join, and every line typed is broadcast to the whole room.

## Building & Running

The engine library must be installed first (once per engine update):

```bash
make -f Makefile.debian install   # from the repo root; sudo if needed
```

Then from this directory:

```bash
make            # build and launch (or `make bin/netchat` to build only)
```

Run a room and a joiner in two terminals:

```bash
./bin/netchat host 5000
./bin/netchat join 127.0.0.1 5000 alice
```

`port` defaults to 5000, `name` defaults to `player`. Type and press Enter to
send; `/quit` leaves.

## What It Demonstrates

| Concept | Where |
|---|---|
| Hosting with fixed slots | `NetServer::Start(port, 8)`, `GetPort()` (0 = ephemeral) |
| Joining by address | `NetClient::Connect(ip, port)` |
| The per-frame pump | `Update()` (timers, keepalives, timeouts) + `Poll()` (receive, flush) in one loop |
| Connect/disconnect callbacks | join/leave prints, including the disconnect *reason* |
| Game messages over chunks | `NetMessageWriter` / `NetMessageReader`: type id first, then fields |
| Reliable messages | `vital = true` - chat must never be dropped |
| Broadcast | `NetServer::Broadcast` fans one message to every client |

The client prints its own messages only after the host echoes them back —
watch your line appear once the round trip completes.

## Wire Up

Every chat line is one chunk: `[varint type=1][varint length][name][varint length][text]`. Nothing about chat is special-cased in the engine - the same `Send`/`Broadcast`/`OnChunk` calls carry any game message (match setup, franchise state, position updates), which is the pattern the networked game loop will use.
