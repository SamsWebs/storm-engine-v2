# Storm Strategy — Jungle Patrol

A top-down strategy scene built as a storm-engine-v2 example. Demonstrates tilemap loading, multiple animated entities, box collider detection, and layered z-index rendering — all driven by the engine's ECS Registry.

## Building & Running

From the `examples/strategy/` directory:

```bash
make        # build and launch
make run    # launch without rebuilding
```

The binary is written to `bin/tanks`.

## How to Play

| Input | Action |
|---|---|
| `D` | Toggle debug collider outlines |
| `ESC` | Quit |

The scene runs autonomously — a friendly truck scrolls right across the map while an enemy tank scrolls left. When they collide the engine's `CollisionSystem` destroys both entities. A scout chopper animates in the top-left corner and a rotating radar dish sits fixed in the top-right.

## Engine Concepts Demonstrated

### TileMapLoader

`TileMapLoader` (`<stormengine2/tilemapLoader.h>`) reads a `.map` file and the matching tileset PNG. For each tile entry it returns a `relativePosition` (grid cell) and `pixelSrcPosition` (source rect in the PNG). `SpawnEntities()` iterates the map and creates one entity per tile:

```cpp
TileMapLoader loader("./assets/tilemaps/jungle.map",
                     "./assets/tilemaps/jungle.png", tileSize);

for (const auto &tile : loader.getMap()) {
    Entity bg = registry_.CreateEntity();
    bg.AddComponent<TransformComponent>(
        glm::vec2(tileScale * tileSize * tile.relativePosition.x, ...),
        glm::vec2(tileScale, tileScale));
    bg.AddComponent<SpriteComponent>("tile-map", tileSize, tileSize, 0,
                                     tile.pixelSrcPosition.x,
                                     tile.pixelSrcPosition.y);
}
```

### Z-Index Layering

`RenderSystem` sorts entities by `SpriteComponent.zIndex` before drawing. The scene uses four layers:

| z-index | Contents |
|---|---|
| 0 | Tilemap background tiles |
| 1 | Friendly truck |
| 2 | Enemy tank, radar dish |
| 3 | Scout chopper (always on top) |

### Tags and Groups

- The chopper is tagged `"chopper"` — retrievable by name for future player control.
- The tank is grouped `"enemies"` and the truck `"friendlies"` — iterable as collections for AI or game-rule logic.

### Built-in Systems Used

| System | Purpose |
|---|---|
| `MovementSystem` | Applies `RigidBodyComponent` velocity to `TransformComponent` each frame |
| `AnimationSystem` | Steps through sprite sheet frames for the chopper (2 frames) and radar (8 frames) |
| `CollisionSystem` | AABB overlap check using `BoxColliderComponent`; destroys both entities on contact |
| `RenderSystem` | Draws all entities sorted by z-index |
| `RenderColliderSystem` | Draws collider outlines in debug mode (toggle with `D`) |

### State Machine

The game uses `GameStateMachine` with a single `PlayState`. All SDL event polling happens exclusively inside `PlayState::processInput()` — the `Game` loop passes straight through — so events are never consumed before the active state sees them.

## Project Structure

```text
strategy/
├── Makefile
├── README.md
├── assets/
│   ├── images/
│   │   ├── chopper.png           ← scout chopper  (32×32, 2-frame strip)
│   │   ├── radar.png             ← radar dish     (64×64, 8-frame strip)
│   │   ├── tank-panther-right.png← enemy tank     (32×32)
│   │   └── truck-ford-right.png  ← friendly truck (32×32)
│   └── tilemaps/
│       ├── jungle.map            ← tile index data
│       └── jungle.png            ← tileset sprite sheet
└── src/
    ├── main.cpp
    ├── game.h / game.cpp
    └── states/
        ├── playState.h
        └── playState.cpp
```
