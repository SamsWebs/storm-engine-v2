# Task 01 — Pong

Two paddles, one ball, one screen. The smallest possible test that a model can
drive the ECS, read input, and resolve collisions by hand.

## Requirements

- 800x600 window, single `PlayState`.
- Left paddle: `W` / `S`. Right paddle: `Up` / `Down`. Clamp both to the screen.
- Ball starts centred with a fixed diagonal velocity.
- Ball bounces off the top and bottom edges, and off both paddles.
- A ball leaving the left or right edge resets to centre.
- `ESC` quits.

## Constraints

- **The scaffold is already in your working directory** — `Makefile`, `src/`,
  `assets/`, `verify.sh`. Edit those files in place; do not create a new
  project directory and do not look for a `references/` path, it is not here.
- Reuse the scaffold's `assets/gfx/tileset.png` for the paddles and ball
  (pick a cell with `SpriteComponent.srcRect`) or draw with SDL primitives.
  Do not require art that does not exist.
- `./verify.sh` must pass.

## Gameplay assertions (level 4)

- The ball's position changes between frames.
- The ball's x-velocity reverses after a paddle overlap.
- Neither paddle leaves the window.

## What this is really testing

Whether the model registers systems before creating entities, calls
`registry_.Update()` before running systems, and — the trap — knows not to use
`CollisionSystem`, which **kills** entities with a `RigidBodyComponent` on
contact instead of bouncing them.
