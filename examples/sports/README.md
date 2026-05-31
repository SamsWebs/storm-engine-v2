# Storm Hockey

A top-down hockey game built as a storm-engine-v2 example. Demonstrates custom ECS components and systems, AI behavior, puck physics, and SDL2 rendering — all wired together through the engine's Registry.

## Building & Running

From the `examples/sports/` directory:

```bash
make        # build and launch
make run    # launch without rebuilding
```

The binary is written to `bin/hockey`.

## How to Play

| Input | Action |
|---|---|
| `W` / `A` / `S` / `D` | Move your skater |
| `Space` | Shoot the puck toward the CPU goal |
| `ESC` | Quit |

Skate close to the puck to pick it up automatically. A status line below the rink confirms when you have possession. Shoot with `Space` — the puck fires toward the right net at full speed.

**First to 3 goals wins.**

## Gameplay

- **You** (blue) start on the left side of the rink.
- **CPU Skater** (red) chases the puck anywhere on the ice and shoots when it gets close to your goal.
- **CPU Goalie** (maroon) stays anchored to the right goal line and tracks the puck's vertical position.
- After each goal the puck and all skaters reset to center ice for a 2-second pause, then play resumes.

## Engine Concepts Demonstrated

### Custom Components

| Component | File | Purpose |
|---|---|---|
| `SkaterComponent` | `src/components/skaterComponent.h` | Team/role enum (`Player`, `AISkater`, `AIGoalie`) and movement speed |
| `PuckComponent` | `src/components/puckComponent.h` | Free-sliding velocity, friction constant, and current owner tag |

### Custom System

**`HockeyPhysicsSystem`** (`src/systems/hockeyPhysicsSystem.h`) requires `TransformComponent` and `PuckComponent`. Each frame it:

1. Moves the puck by its velocity × delta time
2. Applies exponential friction to gradually slow the puck
3. Stops the puck below a minimum speed threshold
4. Bounces it off the top, bottom, left, and right rink walls

Goal detection is intentionally kept in `PlayState` (not the system) to show how game-specific logic can live outside the ECS when that is simpler.

### Engine Systems Used

- **`RenderSystem`** — draws all entities with `TransformComponent` + `SpriteComponent`, sorted by z-index (rink → skaters → puck)

### State Machine

The game uses `GameStateMachine` with a single `PlayState`. Input events are polled exclusively inside `PlayState::processInput()` — the `Game` loop passes straight through — following the pattern established to avoid SDL event queue drain bugs.

## Project Structure

```text
sports/
├── Makefile
├── README.md
├── assets/
│   ├── fonts/
│   │   └── font.ttf
│   └── gfx/
│       ├── rink.png        ← ice surface background (800×600)
│       ├── player.png      ← blue skater (32×32)
│       ├── ai_skater.png   ← red CPU skater (32×32)
│       ├── goalie.png      ← maroon CPU goalie (40×40)
│       └── puck.png        ← black rubber puck (16×16)
└── src/
    ├── main.cpp
    ├── game.h / game.cpp
    ├── components/
    │   ├── skaterComponent.h
    │   └── puckComponent.h
    ├── systems/
    │   └── hockeyPhysicsSystem.h
    └── states/
        ├── playState.h
        └── playState.cpp
```
