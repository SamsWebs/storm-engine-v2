# Task 03 — One-screen platformer

A player who falls, lands, and jumps on a hand-built platform layout. The
hardest of the three: it needs collision *resolution*, which the engine does
not provide.

## Requirements

- 800x600 window, single `PlayState`.
- A hardcoded 2D grid of solid tiles (no `.map` file, no tile editor).
- Gravity pulls the player down; landing on a solid tile stops the fall.
- Left/Right walk. Space jumps, only when grounded.
- The player cannot pass through solid tiles from any direction.
- `ESC` quits.

## Constraints

- **The scaffold is already in your working directory** — `Makefile`, `src/`,
  `assets/`, `verify.sh`. Edit those files in place; do not create a new
  project directory and do not look for a `references/` path, it is not here.
- Use the scaffold's `assets/gfx/tileset.png` for tiles and `player.png` for
  the player.
- `./verify.sh` must pass.

## Gameplay assertions (level 4)

- Released in mid-air, the player's y increases until it rests on a tile.
- Standing on a tile, y stays constant across frames.
- Jumping raises y, then returns to the same resting value.
- Walking into a wall leaves x unchanged.

## What this is really testing

Per-axis resolution. Resolving x and y in one step lets a corner shove the
player sideways along a flat floor. Also that the model does not register
`MovementSystem` for a player it integrates by hand — that double-integrates
the position.
