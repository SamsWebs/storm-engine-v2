---
name: storm-engine-v2
description: >-
  Storm! Engine v2 — a lightweight, ECS-based 2D game engine built on SDL2 (C++17).
  Covers the entity-component-system, game state machine, sprite rendering, tilemaps,
  asset store, networking, and touch input. Use for any project consuming
  libstormenginev2 or the common/ headers directly (games, examples, tools).
risk: none
source: community
date_added: "2026-08-03"
---

# Storm! Engine v2

> **Current release: v1.2.0** — public API stable for the 1.x line.
> Repo: `github.com/WillSams/storm-engine-v2` · License: WTFPL

---

## Architecture Overview

The engine is a C++17 shared library built on SDL2. Games consume it
either as an installed package (`-lstormenginev2`) or by compiling `common/`
directly into the project (common for Switch, Android, and submodule consumers).
The `common/net/` module is a port of Teeworlds 0.7.5 networking (zlib).

**The engine ships no main loop, no Game class, no window management.**
`Game::Run()` is written by the game. The only engine piece in it is
`GameStateMachine`, which forwards each phase to the top state. A state stops
the loop by writing to a `bool&` the Game handed to its constructor — there
is no engine quit API.

### Core Modules

| Module | Header(s) | Purpose |
|--------|-----------|---------|
| **ECS** | `<stormengine2/ecs.h>` | Entity-Component-System registry, entities, systems |
| **Game State Machine** | `<stormengine2/gameStateMachine.h>`, `<stormengine2/states/gameState.h>` | Stack-based state management with deferred cleanup |
| **Asset Store** | `<stormengine2/assetStore.h>` | Texture loading and caching by string ID |
| **Tilemap Loader** | `<stormengine2/tilemapLoader.h>` | Load `.map` files (editor or CSV format) |
| **XML Loader** | `<stormengine2/xmlLoader.h>` | Parse XML asset/entity definitions via tinyxml2 |
| **Logger** | `<stormengine2/logger.h>` | Timestamped, color-coded logging with callback hooks |
| **Networking** | `<stormengine2/net/net.h>` | UDP host/join: reliable chunks, snapshots, kick/ban |
| **Touch Input** | `<stormengine2/input/touchControls.h>`, `<stormengine2/input/virtualGamepad.h>` | SDL-free touch primitives and virtual gamepad layout |

---

## ECS Architecture

### Registry

The `Registry` owns all entities, components, and systems. It's the central hub.

```cpp
Registry registry;

// Entity creation is deferred — call registry.Update() to flush
Entity player = registry.CreateEntity();

// Components are added via the Entity's template methods
player.AddComponent<TransformComponent>(glm::vec2(100, 200), glm::vec2(1, 1), 0.0);
player.AddComponent<RigidBodyComponent>(glm::vec2(0, 0));
player.AddComponent<SpriteComponent>("player", 64, 64, 1);

// Systems are registered with optional constructor args
registry.AddSystem<MovementSystem>();
registry.AddSystem<RenderSystem>();
registry.AddSystem<AnimationSystem>();
registry.AddSystem<CollisionSystem>();
```

**Critical:** Entity creation and destruction are **deferred**. Always call
`registry.Update()` at the start of your state's `update()` method before
running any systems.

### ECS Traps

These are the biggest correctness traps — understand them before writing ECS code:

1. **System membership is computed exactly once per entity** — when `Registry::Update()`
   flushes `entitiesToBeAdded`. `AddComponent`/`RemoveComponent` only flip signature
   bits; they **never re-evaluate system membership**. Adding a component to a live
   entity will not get it into a matching system; removing one will not take it out.
   The only fix is **kill-and-recreate**.

2. **`AddSystem<T>()` only constructs and registers the system** — it never touches
   entities. A system registered after entities were already flushed starts empty
   and stays empty. Always register systems before creating entities.

3. **`MAX_COMPONENTS = 32` is a process-wide cap** — `IComponent::nextId` is a single
   static. A 33rd component type anywhere in the binary makes `Signature.set(id)` throw.

4. **Component storage is dense, not sparse** — one `std::vector<T>` per type, indexed
   directly by entity id. Memory per component type is O(highest entity id). Every
   component type must be **default-constructible**.

5. **`Entity` constructor does not initialize `registry`** — only `Registry::CreateEntity()`
   sets it. `Entity(88)` constructed directly has an indeterminate pointer; any
   `GetComponent`/`AddComponent`/`Kill`/`Tag` on it is UB.

6. **No system scheduler, no virtual `System::Update`** — each concrete system declares
   its own non-virtual `Update` with a bespoke signature. The game state calls each by
   name in an order it chooses.

7. **`Registry::AddEntityToSystem` (singular) is declared but never defined** — calling
   it is a link error. The real one is `AddEntityToSystems`.

8. **The `Logger` writes to `std::cout` on every entity creation and component add** —
   ECS-heavy frames do synchronous console I/O.

### Built-in Components

| Component | Header | Fields |
|-----------|--------|--------|
| `TransformComponent` | `components/transform.h` | `position` (glm::vec2), `scale` (glm::vec2), `rotation` (double, degrees) |
| `RigidBodyComponent` | `components/rigidBody.h` | `velocity` (glm::vec2, px/sec) |
| `SpriteComponent` | `components/sprite.h` | `assetId`, `width`, `height`, `zIndex`, `isFixed`, `flip`, `srcRect`, `offset` |
| `AnimationComponent` | `components/animation.h` | `numFrames`, `frameSpeedRate`, `vertical`, `isLooped`, `frameOffset` |
| `BoxColliderComponent` | `components/boxCollider.h` | `width`, `height`, `offset` (glm::vec2) |

### Built-in Systems

| System | Requires | What it does |
|--------|----------|-------------|
| `MovementSystem` | Transform + RigidBody | Moves entities by `velocity * deltaTime` |
| `RenderSystem` | Transform + Sprite | Draws sprites sorted by `zIndex`, supports camera offset |
| `AnimationSystem` | Sprite + Animation | Advances sprite sheet frames (horizontal or vertical) |
| `CollisionSystem` | Transform + BoxCollider | AABB collision detection; kills entities with RigidBody on contact |
| `RenderColliderSystem` | Transform + BoxCollider | Debug overlay: draws collider outlines in green |

### Custom Components and Systems

Components are plain structs with no base class. Systems extend `System` and
call `RequireComponent<T>()` in their constructor.

```cpp
// Custom component — no base class needed
struct HealthComponent {
    int maxHp = 100;
    int hp = 100;
    HealthComponent() = default;
    HealthComponent(int max) : maxHp(max), hp(max) {}
};

// Custom system
class HealthSystem : public System {
public:
    HealthSystem() { RequireComponent<HealthComponent>(); }
    void Update() {
        for (auto &entity : GetSystemEntities()) {
            if (entity.GetComponent<HealthComponent>().hp <= 0)
                entity.Kill();
        }
    }
};
```

### Tags and Groups

```cpp
player.Tag("player");
enemy.Group("enemies");

// Retrieve by tag (throws if missing)
Entity p = registry.GetEntityByTag("player");

// Iterate a group safely
if (registry.DoesGroupExist("enemies")) {
    for (auto &e : registry.GetEntitiesByGroup("enemies")) { ... }
}
```

---

## Game State Machine

Stack-based state management. States can be pushed, popped, or replaced.

```cpp
gameStateMachine.changeState(new PlayState(...));  // replace current
gameStateMachine.pushState(new PauseState(...));   // push on top (pauses current)
gameStateMachine.popState();                       // return to previous
```

Each state inherits from `GameState` and implements:

```cpp
class MyState : public GameState {
public:
    void processInput() override;
    void update() override;
    void render() override;
    bool onEnter() override;
    bool onExit() override;
    void resume() override;  // optional: called when popped state above is removed
    std::string getStateID() const override { return "MY_STATE"; }
};
```

**Important:** `GameState` already includes all engine headers (ecs.h,
assetStore.h, all systems). Don't re-include them in state headers.

**Critical:** Never call `SDL_PollEvent` in both `Game::ProcessInput` and a
state's `processInput`. The event queue is shared — let the active state own
all event polling.

### Deferred State Deletion

When `changeState` or `popState` is called from inside a state (the normal
pattern), the discarded state is **not deleted immediately** — it's placed in
a defunct list and swept at the start of the next `processInput`/`update`.
This prevents use-after-free when a state deletes itself mid-call.

Rules to code against:

- **`render()` does not sweep** — a state discarded during `update()` stays
  allocated through the following `render()`, freed at the next `processInput()`.
- **Return immediately** after calling `changeState`/`popState` on yourself —
  your object is already off the stack and `onExit()` has already run.
- **`onExit()` must be idempotent** — it can run twice (machine call + destructor).
- **`changeState` to the same `getStateID()`** is a no-op that deletes the
  rejected new state **inline**, not deferred.
- **Initialize in `onEnter()`, tear down in `onExit()`** — not the ctor/dtor.
  `changeState` calls `onEnter()` after pushing; `clean()` calls `onExit()`
  before deleting.
- **The machine owns every state pointer** — pass `new`-allocated states and
  never delete them yourself.

---

## Game Loop Pattern

The `Game` class owns the window, renderer, and main loop. It delegates to
the state machine. **The engine ships no Game class** — the game writes it.

Timestep is variable dt with a 60 FPS **cap**: each state computes
`MILLISECS_PER_FRAME - elapsed` and `SDL_Delay`s the remainder. Nothing
enforces a minimum frame rate. Games typically stack two throttles:
`SDL_RENDERER_PRESENTVSYNC` *and* the state's own delay budget.

There is **no keyboard or gamepad abstraction**. `common/input/` contains
only touch primitives. Keyboard/quit handling is raw `SDL_PollEvent` inside
each state's `processInput()`.

```cpp
void Game::Run() {
    Initialize();
    while (isRunning) {
        ProcessInput();  // gameStateMachine.processInput()
        Update();        // gameStateMachine.update()
        Render();        // gameStateMachine.render()
    }
}
```

### Fixed Timestep in States

States manage their own frame timing:

```cpp
void PlayState::update() {
    int wait = MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame);
    if (wait > 0 && wait <= MILLISECS_PER_FRAME) SDL_Delay(wait);
    double deltaTime = (SDL_GetTicks() - millisecondsPreviousFrame) / 1000.0;
    millisecondsPreviousFrame = SDL_GetTicks();

    registry_.Update();  // flush deferred adds/kills FIRST
    registry_.GetSystem<MovementSystem>().Update(deltaTime);
    registry_.GetSystem<AnimationSystem>().Update();
    registry_.GetSystem<CollisionSystem>().Update();
}
```

`MILLISECS_PER_FRAME` is defined as `1000 / 60` (targeting 60 FPS) in
`gameState.h`.

---

## Asset Store

Textures are loaded and cached by string ID. `GetTexture` returns `nullptr`
for a missing id (it does not throw) — null-check it.

```cpp
assetStore->AddTexture(renderer, "player", "./assets/gfx/player.png");
SDL_Texture *tex = assetStore->GetTexture("player");
assetStore->ClearAssets();  // free all (also called in destructor)
```

The `AssetStore_Ptr` (`std::unique_ptr<AssetStore>`) is typically created in
`Game` and moved into the first state via `std::move`. Pass raw pointers or
references to subsequent states.

---

## Tilemap Loader

Loads `.map` files in two formats:
- **Editor format** (space-separated) — produced by the built-in tile editor
- **CSV format** — legacy format requiring a companion PNG file

```cpp
TileMapLoader loader("level.map", "", 32);  // editor format, 32px tiles
const Map &tiles = loader.getMap();
```

Each `Tile` has: `relativePosition`, `pixelSrcPosition`, `scale`, `zIndex`,
`assetId`, `hasCollider`, `colliderW`, `colliderH`.

---

## XML Loader

Parses XML files for texture and entity definitions.

```cpp
XmlLoader loader("assets/game.xml");
auto textures = loader.GetTextures("PLAY_STATE");
auto objects = loader.GetObjects("PLAY_STATE");

// Convenience: load textures directly into AssetStore
LoadTexturesFromXml("assets/game.xml", "PLAY_STATE", "./assets/",
                    renderer, assetStore, &logger);
```

XML structure:
```xml
<States>
  <PLAY_STATE>
    <TEXTURES>
      <texture filename="player.png" ID="player"/>
    </TEXTURES>
    <OBJECTS>
      <object type="player" x="0" y="0" width="32" height="32"
              textureID="player" numFrames="4" zIndex="1"/>
    </OBJECTS>
  </PLAY_STATE>
</States>
```

---

## Networking (UDP)

The net module provides host/join LAN play over UDP.

| Class | Purpose |
|-------|---------|
| `NetServer` | Host a game: slots, handshake, kick/ban/timeout |
| `NetClient` | Join a server, keep snapshots for prediction |
| `NetConnection` | Reliable transport: vital chunks, acks, resends |
| `NetSnapshot` / `NetSnapshotDelta` | Tick state replication with per-client deltas |
| `NetMessageWriter` / `NetMessageReader` | Game-defined message packing |
| `NetVarInt` | Variable-length integer encoding for compact packets |

See `docs/networking.md` for the wire format and integration recipes.

### Networking Rules

These traps require tracing multiple files to discover:

- **Call `Update()` before `Poll()` every frame.** `NetConnection` caches the
  clock in `nowMs_`, written only by `Update(nowMs)`. A Poll-only loop has a
  frozen clock: RTT reads 0 and timeouts never fire.
- **`NetChunk::data` points into per-connection scratch** that the next `Feed()`
  overwrites — copy anything you need past the callback.
- **Single-threaded** — no `<thread>`/`<mutex>`/`<atomic>`. All callbacks fire
  synchronously on the thread calling `Poll()`/`Update()`.
- **Every vital `Send` is its own datagram** — broadcasting N reliable messages
  per tick costs N datagrams per client.
- **Overflowing the unacked-vital window kills the connection** (96 entries or
  16 KB), it does not block or drop.
- **`NetSnapshot` is two-phase:** `AddItem` only before `Finish()`,
  `FindItem`/`GetItemByIndex` only after. An empty delta base must still have
  `Finish()` called on it.
- **Zero coupling to the ECS or the engine tick.** Snapshots are flat arrays;
  the game hand-marshals ECS components in and out. Nothing in `net/` drives a
  tick — each net example paces itself.
- **Hard ceilings:** chunk ≤ 1200 bytes, datagram ≤ 1400, snapshot ≤ 256 items /
  2048 int32s, prediction cache = 16 ticks (~267 ms at 60 Hz).
- **`-fno-exceptions` on Switch** — the ECS throws through `std::map::at`
  (`GetSystem`, `GetEntityByTag`, `GetEntitiesByGroup`). On Switch those become
  `abort`, not catchable errors.

---

## Touch Input

SDL-free primitives for mobile controls, tested via specs.

### Simple Three-Zone Scheme

```cpp
TouchZones zones = MakeDefaultZones(windowW, windowH);
TouchInput input = EvalTouches(zones, touchPoints, fingerCount);
// input.left, input.right, input.jump
```

### Virtual Gamepad (d-pad + action diamond)

```cpp
VPadLayout layout = MakeVPadLayout(windowW, windowH);            // Xbox lettering
VPadLayout snes  = MakeVPadLayout(windowW, windowH, VPadStyle::Snes);
VPadState state = EvalVPad(layout, touchPoints, fingerCount);
// state.up/down/left/right, state.a/b/x/y
```

Xbox lettering (the default) is Y top, X left, B right, A bottom; SNES is
X top, Y left, A right, B bottom. Both put the four touch targets in the same
places — only which letter sits where changes.

D-pad uses 8-way angle sectors (diagonals set two flags). Action buttons are
a XBOX-style diamond: Y top, X left, B right, A bottom.

---

## Platforms

| Platform | How it works |
|----------|-------------|
| **Linux** | `.deb` package or build from source via `Makefile.debian` |
| **Nintendo Switch** | devkitPro + SDL2 portlibs, compiles engine into `.nro` |
| **Android** | Gradle + CMake + NDK, engine compiled into JNI library via `SDLActivity` |
| **Windows/WSL** | WSL2 with same `apt` prerequisites; native Windows not officially supported |

When consuming as a submodule (Switch, Android, or game-specific), the engine
is compiled directly into the game binary — no shared library to distribute.

---

## Build System

### Engine (installed library)

**There is no plain `Makefile` at the repo root.** Bare `make` fails — every
root invocation needs `-f Makefile.debian`.

```bash
make -f Makefile.debian              # default: clean -> test -> build .so
make -f Makefile.debian target       # build ONLY ./bin/libstormenginev2.so
make -f Makefile.debian test         # build ./bin/tests and run it
make -f Makefile.debian test-target  # build tests without running
make -f Makefile.debian run-test     # run already-built tests (no recompile)
make -f Makefile.debian clean        # rm ./bin/* and every *.o under repo root
sudo make -f Makefile.debian install # .so + headers to /usr/local
```

The default goal **runs the whole spec suite before building the library** —
a failing spec aborts the `.so` build. Use `test` for iteration, `target` when
you only want the library.

`install` has **no prerequisites** — it will happily install a stale `.so`.
Build `target` first.

### Build System Hazards

- **`clean` has repo-wide blast radius.** `ROOT_DIR` derives from `base.mk`'s
  own realpath, so `cd examples/puzzle && make clean` deletes every `*.o` in
  the repository. Since `examples.mk` is `all: clean $(TARGET)`, building any
  one example wipes every other example's objects.
- **No header dependency tracking on desktop.** No `-MMD`/`-MP`. Editing a
  header does not rebuild dependents; you get silently stale objects. This is
  why every `all` starts with `clean`.
- **The Switch build lives in `examples/nx-platformer/`.** A dead `Makefile.nx`
  at the repo root used to shadow it; it was deleted (P42) because it recursed
  into a root `Makefile` that does not exist.
- **`cd editor && make` launches the editor** — the link rule executes the
  binary. Examples do *not* auto-launch.
- **GTK3 and Lua are linked unconditionally**, even for headless networking
  examples. `pkg-config gtk+-3.0` must resolve or nothing compiles.

### Tests

Framework is **Igloo + snowhouse** (BDD `Describe`/`It`), not gtest/Catch2.
Specs live in `specs/` mirroring the source tree; they `#include` engine
headers by relative path, so the suite always tests the working tree, never
the installed library. Test sources are globbed — a new `specs/<area>/<name>.spec.cpp`
is picked up with zero build-file edits.

```bash
./bin/tests            # must run from repo root (specs hardcode ./specs/assets/...)
./bin/tests --help     # option list
```

**Igloo has no CLI filtering.** To run a single test, either rebuild with one
spec file or use `It_Only(...)`/`Describe_Only(...)` source-level selection.

### Games (submodule consumers)

Games typically have their own Makefile that compiles `common/*.cpp` from the
submodule alongside their game source. See `examples/platformer/Makefile` and
`base.mk` / `examples.mk` for the pattern.

**`nx-platformer` compiles only `common/*.cpp` (a non-recursive glob)** — so
`common/net/` is silently absent from the Switch build. Watch for the same
pattern in any game Makefile that globs engine sources by hand.
`android-platformer` had the identical defect and now uses `GLOB_RECURSE`, so
networking builds there.

### Dependencies

- SDL2, SDL2_image, SDL2_ttf, SDL2_mixer
- GLM (math)
- tinyxml2 (XML loading)
- Igloo + snowhouse (test framework, must be built from source)
- GTK3 and Lua (linked unconditionally on desktop, even for headless examples)

---

## Key Design Decisions

1. **ECS over inheritance** — entities are IDs, components are plain data, systems are logic. No deep inheritance hierarchies.
2. **Deferred entity lifecycle** — creation and destruction are batched and flushed on `registry.Update()` to avoid invalidating iterators mid-loop.
3. **System membership is computed once** — `AddComponent`/`RemoveComponent` only flip bits; they never re-evaluate system membership. Kill-and-recreate is the only fix.
4. **State machine owns events** — `SDL_PollEvent` is called only in the active state, never in the game loop.
5. **Deferred state deletion** — discarded states survive the `changeState`/`popState` call that removes them, preventing use-after-free.
6. **No main loop, no Game class** — the engine ships `GameStateMachine` only; the game writes the loop, window, and renderer setup.
7. **SDL-free testable logic** — touch input, virtual gamepad, and net message packing are pure C++ with no SDL dependency.
8. **Camera-aware rendering** — `RenderSystem` accepts an optional `SDL_Rect*` camera; `isFixed` sprites ignore it (for HUD/UI).
9. **Geometric pool growth** — component pools grow 2x to avoid O(n²) reallocation.
10. **Two consumption modes** — installed `.so` (desktop) or compile `common/` directly (Switch, Android, submodules). Editing `common/` changes desktop builds only after `make install`.

---

## Common Patterns

### Creating an Animated Player

```cpp
Entity player = registry.CreateEntity();
player.Tag("player");
player.AddComponent<TransformComponent>(glm::vec2(100, 300), glm::vec2(1, 1), 0.0);
player.AddComponent<RigidBodyComponent>(glm::vec2(0, 0));
player.AddComponent<SpriteComponent>("player", 64, 64, 1);
player.AddComponent<AnimationComponent>(4, 10, false, true);  // 4 frames, 10 FPS, horizontal, looped
player.AddComponent<BoxColliderComponent>(64, 64);
```

### State Transition from Within a State

```cpp
void PlayState::processInput() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_p) {
            gameStateMachine_->pushState(new PauseState(...));
            return;
        }
    }
}
// The PlayState is NOT deleted — it stays on the stack, paused.
// When PauseState pops, PlayState::resume() is called.
```

### Camera for Follow/Scrolling

```cpp
// In your state's render():
SDL_Rect camera = {camX, camY, windowWidth, windowHeight};
registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_, &camera);
```

---

## 2D Game Development Principles

General 2D game dev principles, mapped to how Storm Engine v2 implements them.

### Sprite Systems

| Principle | Storm Engine v2 implementation |
|-----------|-------------------------------|
| **Atlas** — combine textures, reduce draw calls | Load individual PNGs via `AssetStore::AddTexture`; the engine does not atlas automatically. Use `srcRect` on `SpriteComponent` to pick regions from a sprite sheet. |
| **Animation** — frame sequences (8-24 FPS) | `AnimationComponent` + `AnimationSystem` cycle sprite sheet frames. Set `frameSpeedRate` (FPS), `numFrames`, `vertical` (sheet orientation), `isLooped`. |
| **Pivot** — rotation/scale origin | `TransformComponent.rotation` (degrees) and `scale` apply around the entity's position. `SpriteComponent.offset` shifts the draw position relative to transform. |
| **Layering** — z-order control | `SpriteComponent.zIndex` — `RenderSystem` sorts entities by `zIndex` before drawing. Higher values render on top. |

**Animation principles:** squash and stretch for impact, anticipation before action, follow-through after action. The engine handles frame cycling; the game supplies the sprite sheet art.

### Tilemap Design

| Principle | Storm Engine v2 implementation |
|-----------|-------------------------------|
| **Tile size** — 16x16, 32x32, 64x64 | `TileMapLoader` constructor takes `tileSize` (default 32). The JRPG example uses 8 to preserve exact editor pixel coordinates. |
| **Auto-tiling** — use for terrain | Not built in. The tile editor is manual paint/erase. Auto-tiling is a game-side concern. |
| **Collision** — simplified shapes | `Tile.hasCollider` + `colliderW`/`colliderH` on each tile. The engine spawns `BoxColliderComponent` entities; `CollisionSystem` does AABB. |

| Layer | Content | Engine support |
|-------|---------|---------------|
| Background | Non-interactive scenery | Tiles with `zIndex = 0`, no collider |
| Terrain | Walkable ground | Tiles with `hasCollider = true` |
| Props | Interactive objects | Game-defined entities with `BoxColliderComponent` |
| Foreground | Parallax overlay | Tiles with higher `zIndex`; parallax is game-side |

### 2D Physics

| Shape | Use Case | Engine support |
|-------|----------|---------------|
| Box | Rectangular objects | `BoxColliderComponent` — built in, AABB only |
| Circle | Balls, rounded | Not built in — implement as custom component + custom system |
| Capsule | Characters | Not built in — implement as custom component + custom system |
| Polygon | Complex shapes | Not built in — implement as custom component + custom system |

- **Pixel-perfect vs physics-based:** pick one approach per game. The engine's `CollisionSystem` kills entities with `RigidBodyComponent` on contact — it's a simple arcade collision, not a physics solver.
- **Fixed timestep for consistency:** the engine uses variable dt with a 60 FPS cap (`SDL_Delay` on the remainder). For deterministic simulation (e.g., replay systems), games should implement their own fixed timestep.
- **Layers for filtering:** use entity groups (`registry.GroupEntity`) to partition entities for collision logic.

### Camera Systems

| Type | Use | Engine support |
|------|-----|---------------|
| **Follow** | Track player | Game-side: update `SDL_Rect camera` each frame, pass to `RenderSystem::Update(renderer, assetStore, &camera)` |
| **Look-ahead** | Anticipate movement | Game-side: offset camera based on velocity direction |
| **Multi-target** | Two-player | Game-side: average or bound entity positions |
| **Room-based** | Metroidvania | Game-side: snap camera to room bounds |
| **Static** | Board games, modal skill-checks | Pass `nullptr` for camera, or use `SpriteComponent.isFixed = true` for HUD elements that ignore camera |

**Screen shake:** short duration (50-200ms), diminishing intensity, use sparingly. Implement by adding a random offset to the camera rect — game-side, not engine-built.

**Logical resolution scaling:** use `SDL_RenderSetLogicalSize` to letterbox a fixed logical resolution onto any display. The Android example does this; desktop games should too for consistent gameplay across resolutions.

### Genre Patterns

The engine ships 7 example games covering distinct genres. Each demonstrates
different engine capabilities and game-side patterns.

#### Platformer

- **Coyote time** (leniency after edge) — game-side timer in your state's `update()`
- **Jump buffering** — game-side input queue
- **Variable jump height** — game-side: track button hold time, modify `RigidBodyComponent.velocity.y`
- **Tile collision** — `BoxColliderComponent` on tiles + entities; `CollisionSystem` kills on contact, so platformers typically need a custom collision system that resolves instead of killing

The engine's `platformer` example demonstrates the basic pattern: `TransformComponent` + `RigidBodyComponent` + `SpriteComponent` + `AnimationComponent` + `BoxColliderComponent`. The `nx-platformer` and `android-platformer` variants show the same game on Switch and Android.

#### Shooter (Side-scrolling shoot-em-up)

- **Bullet spawning** — create entities on input, add `RigidBodyComponent` with fixed velocity, `Kill()` when off-screen
- **Periodic enemy waves** — spawn entities on a timer, use tags/groups to distinguish factions
- **Scrolling background layers** — multiple `SpriteComponent` entities at different `zIndex` values, scroll at different rates for parallax (game-side)
- **Collision as gameplay** — `CollisionSystem` kills on contact, which works for arcade-style "one hit = death" shooters

The engine's `shooter` example (Alien Attack) demonstrates this pattern.

#### Puzzle (Grid-based / falling blocks)

- **Grid logic is entirely game-side** — the engine has no grid abstraction; represent the board as a 2D array in your state
- **Entity reuse** — the `puzzle` example reuses a pool of block entities rather than creating/destroying each frame, avoiding `registry.Update()` churn
- **Custom components for game state** — e.g., `CellComponent` with grid coordinates, `ShapeComponent` for tetromino identity
- **SDL_ttf for text** — score, level, next-piece preview. The `puzzle` example demonstrates SDL_ttf integration
- **No physics needed** — blocks snap to grid; `RigidBodyComponent` and `CollisionSystem` are typically unused

The engine's `puzzle` example (Storm Tetris) demonstrates custom ECS components, entity reuse, and SDL_ttf rendering.

#### JRPG (Tile-based RPG)

- **Tile-based world** — `TileMapLoader` with small tile size (the `jrpg` example uses 8px to preserve exact editor coordinates)
- **NPC interaction** — game-side proximity check against tagged entities, trigger dialogue state
- **Typewriter dialogue** — game-side text rendering with SDL_ttf, character-by-character reveal
- **State transitions** — push a `DialogueState` over the `PlayState` for conversations; pop when done
- **No real-time physics** — movement is grid-based or tile-based, not velocity-driven

The engine's `jrpg` example demonstrates tile-based world loading, NPC interaction, and typewriter dialogue.

#### Sports (Top-down action)

- **Custom AI components** — player decision-making, positioning tables, reaction logic (e.g., the `sports` example's hockey AI)
- **Puck/ball physics** — custom component for the game object with velocity, friction, bounce — the engine's `RigidBodyComponent` + `MovementSystem` handle basic velocity, but sports games need custom collision resolution (not kill-on-contact)
- **Team management** — use groups to partition teams (`registry.GroupEntity`), query with `GetEntitiesByGroup`
- **Camera follows the play** — center on the ball or midpoint of key entities
- **Set pieces** — kickoff, throw-in, etc. are game states or sub-states within the match state

The engine's `sports` example (Storm Hockey) demonstrates custom ECS components, AI behavior, and puck physics.

#### Strategy (Top-down tactical)

- **Tilemap-driven terrain** — `TileMapLoader` for the map, tiles with colliders for obstacles
- **Multiple animated entities** — units with `SpriteComponent` + `AnimationComponent` at various `zIndex` values for layered rendering
- **Box collider detection** — `BoxColliderComponent` for unit selection, movement validation
- **Layered z-index rendering** — background tiles at low `zIndex`, units at mid, UI at high or `isFixed`
- **Turn-based or real-time** — the engine doesn't enforce either; game-side logic controls the tick

The engine's `strategy` example (Jungle Patrol) demonstrates tilemap loading, multiple animated entities, and layered z-index rendering.

#### Netplay Board Game (Turn-based multiplayer)

- **Authoritative host** — `NetServer` validates every move; clients send input, host sends result
- **Full-state broadcast** — simpler than delta snapshots for turn-based games; `NetSnapshot` with one item per board piece
- **Late joiner sync** — full-state message brings new clients up to date immediately
- **Turn flow over reliable chunks** — `NetMessageWriter`/`NetMessageReader` for move encoding; vital chunks guarantee delivery
- **No prediction or rollback** — turn-based games don't need it; the cheapest correct networking approach
- **ECS for board representation** — each piece is an entity with position and type components

The engine's `netplay-checkers` example demonstrates graphical, authoritative-netplay checkers with full-state sync.

---

## Anti-Patterns

| Don't | Do |
|-------|-----|
| Call `SDL_PollEvent` in both Game and State | Let the active state own all event polling |
| Forget `registry.Update()` before systems | Always flush deferred adds/kills first |
| Re-include engine headers in state headers | `GameState` already includes them all |
| Move `AssetStore_Ptr` to multiple states | Move once to first state, pass raw ptr/ref after |
| Delete states inline on transition | Use the state machine's push/pop/change (deferred deletion) |
| Add components before registering systems | Register systems first, then create entities |
| `AddComponent` on a live entity to get it into a system | Kill and recreate the entity — membership is computed once |
| Call `make` without `-f Makefile.debian` at repo root | Always use `-f Makefile.debian` |
| Run `./bin/tests` from outside the repo root | Run from repo root — specs hardcode relative paths |
| Forget `Update()` before `Poll()` in networking | `NetConnection` caches the clock in `Update` only |
| Keep `NetChunk::data` past the callback | Copy it — the next `Feed()` overwrites the scratch buffer |
| Call `install` without building first | Build `target` first — `install` has no prerequisites |
| Use separate textures when a sprite sheet would do | Use `SpriteComponent.srcRect` to pick regions from a sheet |
| Use complex collision shapes for simple objects | Use `BoxColliderComponent` (AABB) — add custom shapes only when needed |
| Jittery camera | Smooth camera following with interpolation in your state's `update()` |
| Mix pixel-perfect and physics-based collision | Pick one approach per game |
| Forget to null-check `AssetStore::GetTexture` | It returns `nullptr` for missing IDs, not an exception |

---

## When to Use

Use this skill when working on any project that consumes Storm! Engine v2 —
whether as an installed library, a git submodule, or by compiling `common/`
directly. This includes writing game states, custom components/systems,
tilemap-based levels, networking code, or touch input for mobile targets.

## Naming Conventions

Three member-naming schemes coexist and are load-bearing:
- Engine ECS core: bare names (`numEntities`)
- `GameStateMachine`: `m_` prefix (`m_gameStates`)
- Game/state code: trailing underscore (`renderer_`)

Method casing is split: PascalCase in ECS/AssetStore/Logger (`CreateEntity`,
`AddTexture`), camelCase in the state machine and `GameState` virtuals
(`pushState`, `processInput`, `onEnter`) — overrides must match camelCase.

Games always include the engine with angle brackets
(`#include <stormengine2/ecs.h>`); quoted/relative includes are reserved for
the game's own headers.

## Limitations

- This skill covers the engine's public API and common patterns. For
  game-specific logic (e.g., sports simulation, AI, match flow), refer to the
  consuming project's own code and documentation.
- The engine does not provide audio playback — games handle SDL_mixer
  initialization and music/SFX themselves.
- No built-in physics engine — `CollisionSystem` does simple AABB detection
  only. Games needing complex physics implement their own.
- No built-in scene editor beyond the tile map editor. Entity placement is
  code-driven or XML-driven.
- The engine ships no main loop, no Game class, no window management.
- No keyboard or gamepad abstraction — only touch primitives in `common/input/`.
- `common/net/` is absent from Switch and Android builds (non-recursive glob).
- The editor has its own `SpriteComponent` copy that shadows the engine's.


