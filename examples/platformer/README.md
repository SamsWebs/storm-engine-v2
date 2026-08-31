# Storm Platformer

A scrolling 2-D platformer built as a storm-engine-v2 example. Demonstrates tilemap loading with `TileMapLoader`, tile-based physics with gravity and AABB resolution, and a scrolling camera - all without modifying the engine.

![Storm Engine v2 platformer example](screenshot.png)

## Building & Running

From the `examples/platformer/` directory:

```bash
make        # build
make run    # launch the already-built binary
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

The level is 40 × 28 tiles of 40 px each - 1600 × 1120 px, so the camera scrolls on both axes. The ground steps between four heights with floating platforms above it - jump between them to explore. Falling off the bottom respawns the player at the start.

## Engine Concepts Demonstrated

### TileMapLoader

`platformer.map` is in the **editor format** - one space-separated record per tile carrying the tileset id, the source rect, the world position and a collider flag:

```text
group assetId tileW tileH srcX srcY zIndex worldX worldY scaleX scaleY collider [colW colH offX offY] animated
```

`TileMapLoader` auto-detects the format by peeking at the first non-space character (a letter means editor format, a digit means legacy CSV), so the tileset PNG argument is left empty - `srcX`/`srcY` are already in the file. `worldX`/`worldY` step by the 16 px source tile size; the loader divides by the `tileSize` it was handed to get the grid cell, and `SpawnTiles()` re-multiplies by the on-screen `TILE_PX` (16 × 2.5 = 40).

`SpawnTiles()` iterates `loader.getMap()` and spawns an entity for every tile, but **only the tiles the map marks with a collider are solid** - treating every painted tile as ground turned clouds, bushes and rocks into walls, which is why the level could carry no decoration:

```cpp
TileMapLoader loader("./assets/tilemaps/platformer.map", "", TILE_SIZE);

for (const auto &tile : loader.getMap()) {
    int col = tile.relativePosition.x;
    int row = tile.relativePosition.y;

    if (tile.hasCollider && /* in bounds */)
        solidGrid_[row][col] = true;   // mirror for physics

    Entity e = registry_.CreateEntity();
    e.Group("tiles");
    e.AddComponent<TransformComponent>(...);
    e.AddComponent<SpriteComponent>(tile.assetId, TILE_SIZE, TILE_SIZE,
                                    tile.zIndex, false,
                                    tile.pixelSrcPosition.x,
                                    tile.pixelSrcPosition.y);

    if (tile.hasCollider) {
        e.Group("solid");
        e.AddComponent<BoxColliderComponent>(TILE_PX, TILE_PX);
    }
}
```

The shipped map is 295 tiles, 72 of them solid, on z-indices -1 to 2. `assets/tilemaps/TILESET.md` indexes the sheet, and `author_map.py` is the script that authored the map.

### Tile-Based Physics

The engine has no kill-on-contact collision system, and `ContactSystem` only reports overlaps - it never acts on them, so neither is a fit for player-vs-tile interaction by itself. Instead, `PlayState` keeps a `solidGrid_[row][col]` boolean mirror of the map and resolves collisions directly in `ResolvePlayer()`.

The algorithm runs two separate passes each frame:

1. **Horizontal** - move the player by `vx * dt`, then check the two leading-edge corner cells. If either is solid, snap the player flush to the tile boundary.
2. **Vertical** - move the player by `velocity.y * dt` (gravity accumulates when airborne), then check the two leading-edge corner cells. Landing on a tile top sets `isOnGround = true` and zeroes vertical velocity; hitting a ceiling zeroes upward velocity.

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

A `glm::vec2 camera_` is updated each frame to keep the player centred, clamped so the camera never scrolls past the level edges. The level is bigger than the 800×600 window on both axes, so it scrolls vertically as well as horizontally:

```cpp
float targetX = transform.position.x - windowWidth_ / 2.0f
              + (PLAYER_W * PLAYER_SCALE) / 2.0f;
float targetY = transform.position.y - windowHeight_ / 2.0f
              + (PLAYER_H * PLAYER_SCALE) / 2.0f;

camera_.x = std::max(0.0f, std::min(targetX, levelWidth  - windowWidth_));
camera_.y = std::max(0.0f, std::min(targetY, levelHeight - windowHeight_));
```

Because the built-in `RenderSystem` draws at raw world positions, `render()` bypasses it and iterates `GetSystemEntities()` directly, subtracting `camera_` from each destination rect. Off-screen tiles are culled before `SDL_RenderCopyEx` is called.

### State Machine

`Game` uses `GameStateMachine` with a single `PlayState`. All SDL event polling lives exclusively in `PlayState::processInput()` - the `Game` loop is a pure passthrough. All setup (asset loading, tile spawning, player creation) happens in the `PlayState` constructor; `onEnter()` only sets `m_loadingComplete = true`.

Frame pacing is the engine's: `update()` opens with `float dt = static_cast<float>(CapFrameRate());`, the protected `GameState` helper that sleeps out the rest of the 60 FPS budget, clamps a hitch to 50 ms and returns the elapsed seconds.

## Project Structure

```text
platformer/
├── Makefile
├── README.md
├── screenshot.png
├── assets/
│   ├── fonts/
│   │   └── font.ttf                    ← not used by this example
│   ├── gfx/
│   │   └── rabbit.png                  ← 37×1026 strip: 10 idle then 8 walk frames of 37×57
│   └── tilemaps/
│       ├── 16x16-platformer.png        ← tileset sprite sheet (160×384, 10×24 tiles)
│       ├── TILESET.md                  ← index of the sheet, and authoring notes
│       ├── License.txt                 ← tileset licence
│       ├── author_map.py               ← the script that authored platformer.map
│       ├── platformer.map              ← 40×28 editor-format tile map
│       ├── platformer.lua              ← editor project file, never read by the game
│       └── platformer_colliders.map    ← editor collider layer, never read by the game
└── src/
    ├── main.cpp
    ├── game.h / game.cpp
    ├── components/
    │   └── playerComponent.h
    └── states/
        ├── playState.h
        └── playState.cpp
```
