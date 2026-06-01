# Storm Platformer

A scrolling 2-D platformer built as a storm-engine-v2 example. Demonstrates tilemap loading with `TileMapLoader`, tile-based physics with gravity and AABB resolution, and a horizontally scrolling camera — all without modifying the engine.

![Storm Engine v2 platformer example](screenshot.png)

## Building & Running

From the `examples/platformer/` directory:

```bash
make        # build and launch
make run    # launch without rebuilding
```

The binary is written to `bin/platformer`.

## How to Play

| Input | Action |
|---|---|
| `←` / `A` | Move left |
| `→` | Move right |
| `Space` / `↑` / `W` | Jump |
| `D` | Toggle debug collider outlines |
| `ESC` | Quit |

The level is 2000 px wide (50 tiles × 40 px each). Platforms are arranged at four heights — jump between them to explore. Falling off the bottom respawns the player at the start.

## Engine Concepts Demonstrated

### TileMapLoader

`TileMapLoader` reads a dense comma-separated `.map` file and the matching tileset PNG. Each row in the file is a row of tile indices; each index maps to a pixel source rect in the sheet via:

```cpp
col = index % (sheetWidth / tileSize)
row = index / (sheetWidth / tileSize)
srcX = col * tileSize,  srcY = row * tileSize
```

The platformer map (`platformer.map`) uses three tile indices:

| Index | srcX | srcY | Colour | Purpose |
|---|---|---|---|---|
| 8 | 128 | 0 | transparent | Sky / air — skipped entirely, background colour shows through |
| 0 | 0 | 0 | grass-top + brown body | Ground surface row and platforms |
| 91 | 16 | 144 | solid warm brown | Underground fill rows below the surface |

`SpawnTiles()` iterates `loader.getMap()`, skips sky tiles, and treats **everything else as solid** — no need to enumerate every solid tile variant:

```cpp
TileMapLoader loader("./assets/tilemaps/platformer.map",
                     "./assets/tilemaps/16x16-platformer.png", TILE_SIZE);

glm::ivec2 skySrc = loader.pixelPosFromTilePos(SKY_TILE);

for (const auto &tile : loader.getMap()) {
    bool isSky   = (tile.pixelSrcPosition == skySrc);
    bool isSolid = !isSky;

    solidGrid_[row][col] = isSolid;   // mirror for physics
    if (isSky) continue;              // no entity for air

    Entity e = registry_.CreateEntity();
    e.Group("tiles");
    e.AddComponent<TransformComponent>(...);
    e.AddComponent<SpriteComponent>("platformer-tiles", ...);

    if (isSolid) {
        e.Group("solid");
        e.AddComponent<BoxColliderComponent>(TILE_PX, TILE_PX);
    }
}
```

### Tile-Based Physics

The engine's built-in `CollisionSystem` destroys both colliding entities — not suitable for player-vs-tile interaction. Instead, `PlayState` keeps a `solidGrid_[row][col]` boolean mirror of the map and resolves collisions directly in `ResolvePlayer()`.

The algorithm runs two separate passes each frame:

1. **Horizontal** — move the player by `vx * dt`, then check the two leading-edge corner cells. If either is solid, snap the player flush to the tile boundary.
2. **Vertical** — move the player by `velocity.y * dt` (gravity accumulates when airborne), then check the two leading-edge corner cells. Landing on a tile top sets `isOnGround = true` and zeroes vertical velocity; hitting a ceiling zeroes upward velocity.

```cpp
// Vertical resolve (downward case)
int bot = static_cast<int>(transform.position.y + ph - 1) / TILE_PX;
if (IsSolid(left, bot) || IsSolid(right, bot)) {
    transform.position.y = bot * TILE_PX - ph;
    pc.velocity.y  = 0.0f;
    pc.isOnGround  = true;
}
```

### PlayerComponent

A lightweight custom component holding everything the physics loop needs:

```cpp
struct PlayerComponent {
    glm::vec2 velocity    = {0.0f, 0.0f};
    bool      isOnGround  = false;
    bool      facingRight = true;
    float     moveSpeed   = 180.0f;
    float     jumpSpeed   = -480.0f;   // negative = up in SDL coords
    float     gravity     = 900.0f;
};
```

### Scrolling Camera

A `glm::vec2 camera_` is updated each frame to keep the player horizontally centred, clamped so the camera never scrolls past the level edges:

```cpp
float targetX = transform.position.x - windowWidth_ / 2.0f
              + (PLAYER_W * PLAYER_SCALE) / 2.0f;
camera_.x = std::clamp(targetX, 0.0f, levelWidth - windowWidth_);
```

Because the built-in `RenderSystem` draws at raw world positions, `render()` bypasses it and iterates `GetSystemEntities()` directly, subtracting `camera_` from each destination rect. Off-screen tiles are culled before `SDL_RenderCopyEx` is called.

### State Machine

`Game` uses `GameStateMachine` with a single `PlayState`. All SDL event polling lives exclusively in `PlayState::processInput()` — the `Game` loop is a pure passthrough. All setup (asset loading, tile spawning, player creation) happens in the `PlayState` constructor; `onEnter()` only sets `m_loadingComplete = true`.

## Project Structure

```text
platformer/
├── Makefile
├── README.md
├── assets/
│   ├── gfx/
│   │   └── player.png                  ← 16×24 player sprite
│   └── tilemaps/
│       ├── 16x16-platformer.png        ← tileset sprite sheet (160×384, 10×24 tiles)
│       └── platformer.map              ← 50×15 dense tile-index grid
└── src/
    ├── main.cpp
    ├── game.h / game.cpp
    ├── components/
    │   └── playerComponent.h
    └── states/
        ├── playState.h
        └── playState.cpp
```
