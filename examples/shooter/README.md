# Storm Shooter — Alien Attack

A side-scrolling shoot-em-up built as a storm-engine-v2 example. Demonstrates the engine's built-in movement, animation, collision, and render systems working together, plus scrolling background layers, bullet spawning, and periodic enemy waves.

![Storm Engine v2 shooter example](screenshot.png)

## Building & Running

From the `examples/shooter/` directory:

```bash
make        # build and launch
make run    # launch without rebuilding
```

The binary is written to `bin/alienattack`.

## How to Play

| Input | Action |
|---|---|
| `↑` / `↓` / `←` / `→` | Move the player helicopter |
| `Space` | Fire a bullet (250 ms cooldown) |
| `D` | Toggle debug collider outlines |
| `ESC` | Quit |

Enemies spawn from the right edge every 2.5 seconds and scroll left. Shoot them before they pass off the left edge.

## Enemy Types

| Type | Sprite | Speed | Notes |
|---|---|---|---|
| Green helicopter | `helicopter2.png` | Normal | Animated, 5-frame strip, same size as player |
| Alien bug | `enemy1.png` | Fast (1.3×) | Static sprite, scaled up 2× |
| Bat | `enemy3.png` | Slow (0.8×) | Animated, 2-frame strip, scaled up 2× |

## Engine Concepts Demonstrated

### Built-in Systems Used

| System | Purpose |
|---|---|
| `MovementSystem` | Applies `RigidBodyComponent` velocity to `TransformComponent` position each frame |
| `AnimationSystem` | Steps through horizontal sprite sheet frames using `AnimationComponent` |
| `CollisionSystem` | Detects overlapping `BoxColliderComponent` pairs |
| `RenderSystem` | Draws all entities with `TransformComponent` + `SpriteComponent`, sorted by z-index |
| `RenderColliderSystem` | Draws `BoxColliderComponent` outlines (debug mode, toggle with `D`) |

### Scrolling Background

Two cloud entities are spawned side by side, each covering the full screen. Both move left at the same velocity via `RigidBodyComponent`. When a layer scrolls completely off the left edge it is repositioned one full screen-width to the right, creating seamless infinite parallax.

### Sprite Flipping

The player helicopter sprite faces left by default. `SpriteComponent.flip` is set to `SDL_FLIP_HORIZONTAL` after the component is added so it faces the oncoming enemies — a common pattern when your source art points the wrong way.

### Entity Groups and Tags

- Player is tagged `"player"` so it can be retrieved by name for input and bounds-clamping.
- Cloud layers are tagged `"clouds1"` / `"clouds2"` for individual wrap-around resets.
- Bullets are grouped `"bullets"` and enemies `"enemies"` so off-screen despawning can iterate each group without touching unrelated entities.

### State Machine

The game uses `GameStateMachine` with a single `PlayState`. All SDL event polling happens exclusively inside `PlayState::processInput()` — the `Game` loop passes straight through — so events are never consumed before the active state sees them.

## Project Structure

```text
shooter/
├── Makefile
├── README.md
├── assets/
│   └── gfx/
│       ├── helicopter.png    ← player  (640×55, 5-frame strip)
│       ├── helicopter2.png   ← enemy 1 (640×55, 5-frame strip)
│       ├── enemy1.png        ← enemy 2 alien bug  (38×34)
│       ├── enemy3.png        ← enemy 3 bat        (104×35, 2-frame strip)
│       ├── bullet1.png       ← player bullet       (11×11)
│       ├── clouds.png        ← scrolling sky       (640×480)
│       └── smallexplosion.png
└── src/
    ├── main.cpp
    ├── game.h / game.cpp
    └── states/
        ├── playState.h
        └── playState.cpp
```
