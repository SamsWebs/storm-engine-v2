# Storm Checkers (netplay)

A graphical, authoritative-netplay checkers game built as a storm-engine-v2 example. Demonstrates the engine's net module for real gameplay: the host validates every move, a full-state message syncs late joiners, and turns flow over reliable chunks - no prediction, no rollback, the cheapest way to learn networked game rules.

![Storm Engine v2 checkers example](screenshot.png)

## Building & Running

From the `examples/netplay-checkers/` directory:

```bash
make            # build
make run        # launch (host, default port 51235)
```

The binary is written to `bin/netplay-checkers`.

Start the host, then join from other machines or terminals:

```bash
./bin/netplay-checkers host 51235                    # on the host machine
./bin/netplay-checkers <host-ip|localhost> 51235     # on a client machine
```

`<host-ip>` is **the host machine's own LAN address** - substitute it, do not
type it literally. Find it on the host with `hostname -I` (or `ip -4 addr`).
Both processes on the same machine? Use `localhost`.

There is no error for pointing a client at an address nothing is listening on:
the client retries the handshake, the server never sees a packet, and after
about ten seconds the client times out and shuts down. If a client never
reaches the lobby, check the address first.

The first two connected players are seated RED and BLACK; everyone else watches as a spectator. The host starts the game with `S`.

## How to Play

| Input | Action |
|---|---|
| Click a piece | Select it (only your color, only on your turn) |
| Click a destination | Move; if you must capture, only capture moves are accepted |
| `ESC` | Quit |
| `S` (host) | Start the game / rematch |
| `R` (host) | Return to the lobby |
| `Enter` / `Backspace` | Chat or a `/command` (`/name`, `/ff`) |

Rules are standard draughts:

- Pieces move one square diagonally; kings (from promotion) move in both directions.
- Captures are **forced**: if you have a capture, you must take it.
- Multi-jump chains continue automatically - keep clicking the same piece until the chain ends.
- Forfeit any time with `/ff`; if a player disconnects mid-game, the other side wins.
- The host announces every move and the winner in the log and HUD.

## Engine Concepts Demonstrated

- **Net module**: `NetServer`/`NetClient` rooms, reliable ordered `NetMessageWriter` chunks, a custom message protocol (`kMsgLobby`, `kMsgSeat`, `kMsgGame`, `kMsgMove`, `kMsgCmd`, `kMsgChat`), and host-authoritative validation of every move.
- **Late-joiner sync**: the host broadcasts the full `CheckersState` (turn, winner, chain state, 64-cell board) whenever anything changes, so a client that connects mid-game renders the current position.
- **ECS + RenderSystem**: board, pieces, and highlight marks are `Transform` + `Sprite` entities with z-index layering, rebuilt by the host after each move.
- **AssetStore**: PNG textures for board/pieces/marks, TTF HUD font, and SDL_mixer SFX for moves, captures, and the win fanfare (audio degrades gracefully when no device is available).

The server is the same binary as the client - `host`/join is decided by the command line, and the engine's net module runs on both sides.
