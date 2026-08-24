# Task 02 — Collect the coins

A player moving in four directions over a fixed screen, gathering pickups.
Tests entity lifecycle: creation, deferred flush, and killing entities safely.

## Requirements

- 800x600 window, single `PlayState`.
- Player moves with the arrow keys, clamped to the window.
- Exactly 8 coins spawn at fixed (not random) positions.
- Overlapping a coin removes it and increments a counter logged to the console.
- When the last coin is taken, log a completion line. The game keeps running.
- `ESC` quits.

## Constraints

- **The scaffold is already in your working directory** — `Makefile`, `src/`,
  `assets/`, `verify.sh`. Edit those files in place; do not create a new
  project directory and do not look for a `references/` path, it is not here.
- Use the scaffold's existing art.
- No `rand()` — positions must be deterministic so the run is reproducible.
- `./verify.sh` must pass.

## Gameplay assertions (level 4)

- 9 entities exist after the first flush (player + 8 coins).
- Taking a coin decrements the live coin count by exactly 1.
- The completion line is logged exactly once.

## What this is really testing

`Entity::Kill()` is **deferred** — the entity stays alive until the next
`registry_.Update()`. A model that assumes immediate removal will double-count
a coin. Also whether it copies the group vector before killing while iterating
it.

If it uses `ContactSystem` (v1.3.0+) for the pickup test rather than a hand
distance check, the same trap shows up one level down: a killed coin stays in
the system's entity vector, and so can still turn up in a contact, until that
next flush. Registering `CollisionSystem` instead is a fail: it would kill the
player along with the coin.
