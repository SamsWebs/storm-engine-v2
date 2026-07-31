# netrepl

A headless replication demo: the host simulates six players bouncing around a
40×12 field at 60 Hz and streams the world to every client as varint-packed
snapshot *deltas* — the wire only carries what changed. Clients apply each
delta against their confirmed base snapshot and render the field as ASCII.

This is the shape of the networked game loop: authoritative fixed-tick host,
per-client deltas, client-side prediction bases.

## Building & Running

The engine library must be installed first (once per engine update):

```bash
make -f Makefile.debian install   # from the repo root; sudo if needed
```

Then from this directory:

```bash
make            # build and launch (or `make bin/netrepl` to build only)
```

Run the host and one or more clients in separate terminals (Ctrl-C to stop):

```bash
./bin/netrepl host 5000
./bin/netrepl join 127.0.0.1 5000
```

## What It Demonstrates

| Concept | Where |
|---|---|
| Authoritative fixed-tick host | host loop paces itself to 60 Hz with `NetNowMs()` |
| Typed game state | `NetSnapshot::AddItem(type, id, int32s)` — players are type 1, id 0..5 |
| Per-client delta bases | `bases[clientId]` — each client gets a delta vs what *it* confirmed |
| Delta encoding | `NetSnapshotDelta::Create` (0 bytes = nothing changed, send nothing) |
| Delta application | `NetSnapshotDelta::Apply` against the stored base snapshot |
| Prediction cache | `NetClient::StoreSnapshot(tick, snap)` / `GetSnapshot(tick)` |
| Message framing | `[varint type][varint tick][delta bytes]` via writer/reader |

## Reading the Output

Each render shows the tick, the player count, and the field: `.` is empty ice,
digits are players (their id mod 10). The digits drift and bounce — that is
the deltas arriving, applying, and being redrawn a few times per second.

## Mapping to a Game

Replace `AddItem(1, id, &x, 2)` with skaters (`x, y, vx, vy, energy` — up to
2048 int32s per snapshot) and the puck; render with sprites instead of ASCII;
and let clients predict between snapshots using `GetSnapshot(tick)` as the
base. The message pattern (type id + tick + delta) is unchanged.
