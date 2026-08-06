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

> **Current release: v1.2.1** — public API stable for the 1.x line.
> Unreleased work (this branch) is headed for v1.2.2 — see `CHANGELOG.md` `[Unreleased]`.
> Repo: `github.com/WillSams/storm-engine-v2` · License: WTFPL

---

## Architecture Overview

The engine is a C++17 shared library built on SDL2. Games consume it
either as an installed package (`-lstormenginev2`) or by compiling `common/`
directly into the project (common for Switch, Android, and submodule consumers).

A Windows cross-build exists via MinGW-w64 (`Makefile.win`,
`cmake/toolchain-mingw64.cmake`, `examples/examples.win.mk`), producing
`libstormenginev2.dll` plus the spec suite. It is not covered by CI, which
builds `Dockerfile.debian` only.

The `common/net/` module is a port of Teeworlds 0.7.5 networking (zlib).
Android compiles all of `common/` including `common/net/`
(`file(GLOB_RECURSE ...)`); the Switch example enumerates source directories
non-recursively and therefore does not build `common/net/`.

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
| **Touch Input** | `<stormengine2/input/touchControls.h>`, `<stormengine2/input/virtualGamepad.h>` | SDL-free touch primitives and virtual gamepad layout. `MakeVPadLayout(w, h, style)` takes an optional `VPadStyle`, defaulting to `VPadStyle::Xbox` (Y top, X left, B right, A bottom); pass `VPadStyle::Snes` for the older arrangement. Touch-target positions are identical under both — only the lettering moves. |

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
// NOTE: AddSystem takes its ctor args by lvalue reference (`Targs &... args`),
// so temporaries do not bind — bind them to a named variable first:
//   int hp = 5; registry.AddSystem<HealthSystem>(hp);   // OK
//   registry.AddSystem<HealthSystem>(5);                // does not compile
registry.AddSystem<MovementSystem>();
registry.AddSystem<RenderSystem>();
registry.AddSystem<AnimationSystem>();
registry.AddSystem<CollisionSystem>();

// Reference accessor — PRECONDITION: the entity has the component. On a miss it
// logs (throttled) and returns a shared default-constructed fallback.
auto &t = player.GetComponent<TransformComponent>();

// Pointer accessor — silent, returns nullptr on a miss (no pool, id out of
// range, signature bit clear, or a null registry). Use this whenever absence
// is possible; it cannot alias.
if (auto *rb = player.TryGetComponent<RigidBodyComponent>()) {
    rb->velocity.x = 0;
}
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
   The usual fix is **kill-and-recreate**; a live entity can also be refreshed by
   hand with `registry.RemoveEntityFromSystems(e); registry.AddEntityToSystems(e);`
   — both are public. Calling `AddEntityToSystems` alone double-adds the entity to
   systems it already matches, because `System::AddEntityToSystem` push_backs
   unconditionally.

2. **`AddSystem<T>()` only constructs and registers the system** — it never touches
   entities. A system registered after entities were already flushed starts empty
   and stays empty. Always register systems before creating entities.

3. **`MAX_COMPONENTS = 32` is a process-wide cap** — `IComponent::nextId` is a single
   static. A 33rd component type does **not** throw any more: `EcsComponentIdIsValid`
   range-checks the id, logs a throttled error, and the type is then ignored
   everywhere — `RequireComponent` drops the requirement, `AddComponent` /
   `RemoveComponent` no-op, `HasComponent` returns `false`, and `GetComponent`
   returns the fallback. Signature bits are set with `operator[]`, never
   `bitset::set()`, so no throw is emitted into a `-fno-exceptions` game TU.

4. **Component storage is dense, not sparse** — one `std::vector<T>` per type, indexed
   directly by entity id. Memory per component type is O(highest entity id). Every
   component type must be **default-constructible**.

5. **A hand-built `Entity` is inert, not UB** — `Entity::registry` is `nullptr` by
   default and only `Registry::CreateEntity()` sets it. `Entity(88)` built directly
   is safe to call but does nothing: every forwarder null-checks first, then logs a
   throttled error and no-ops (`Kill`/`Tag`/`Group`/`AddComponent`/`RemoveComponent`),
   returns `false` (`HasComponent`/`HasTag`/`BelongsToGroup`), returns `nullptr`
   (`TryGetComponent`), or returns the default-constructed fallback (`GetComponent`).
   It is still a caller bug — it just no longer segfaults.

6. **No system scheduler, no virtual `System::Update`** — each concrete system declares
   its own non-virtual `Update` with a bespoke signature. The game state calls each by
   name in an order it chooses.

7. **`GetComponent<T>()` never throws on a miss — it returns a shared fallback** —
   when the entity has no such component (no pool, id out of range, signature bit
   clear, or a type past `MAX_COMPONENTS`) it logs a throttled error and returns
   `EcsFallbackComponent<T>()`: one thread-local instance per component type,
   re-zeroed per miss. Two misses still hand back references that alias each other.
   Use `Registry::TryGetComponent<T>` / `Entity::TryGetComponent<T>` (silent,
   `nullptr` on a miss) whenever absence is possible.

8. **The `Logger` writes to `std::cout` on every entity creation and component add** —
   ECS-heavy frames still do a console write per operation, but it is buffered:
   `logHelper` terminates lines with `'\n'` and only calls `std::cout.flush()` for
   `LOG_ERROR`. ECS diagnostics are throttled to the first
   `ECS_MAX_DIAGNOSTIC_REPORTS` (4) occurrences per call site and then go quiet,
   and `Logger::messages` is capped at 1000 entries.

9. **Entity ids are recycled, and `Entity` carries no generation counter** —
   `Registry::IsAlive(e)` reports whether the id is currently in use (id created
   and not parked in `freeIds`), and `KillEntity` now ignores a kill for an entity
   that is not alive or is already pending kill this frame, logging a throttled
   error instead of queueing a double-kill. A **stale handle whose id has since
   been recycled still reads as alive** and will kill the new occupant — tracked
   as P5 in `docs/TECH_DEBT.md`. `IsAlive` scans `freeIds`, so it is O(freed ids),
   not O(1).

### Built-in Components

| Component | Header | Fields |
|-----------|--------|--------|
| `TransformComponent` | `components/transform.h` | `position` (glm::vec2), `scale` (glm::vec2), `rotation` (double, degrees) |
| `RigidBodyComponent` | `components/rigidBody.h` | `velocity` (glm::vec2, px/sec) |
| `SpriteComponent` | `components/sprite.h` | `assetId`, `width`, `height`, `zIndex`, `isFixed`, `flip`, `srcRect`, `offset` |
| `AnimationComponent` | `components/animation.h` | ctor args: `numFrames`, `frameSpeedRate`, `vertical` (default `true`), `isLooped` (default `true`), `frameOffset`; also public: `currentFrame`, `startTime` (set to `SDL_GetTicks()` in the ctor), `lastFrame` (non-looped stop frame, 0 = use `numFrames - 1`) |
| `BoxColliderComponent` | `components/boxCollider.h` | `width`, `height`, `offset` (glm::vec2) |

### Built-in Systems

| System | Requires | What it does |
|--------|----------|-------------|
| `MovementSystem` | Transform + RigidBody | Moves entities by `velocity * deltaTime` |
| `RenderSystem` | Transform + Sprite | Draws sprites sorted by `zIndex`; applies the camera offset except to sprites with `isFixed` (HUD/screen-space), and honours `sprite.offset`, `transform.scale`, `transform.rotation` and `sprite.flip` |
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

// GetEntityByTag has a PRECONDITION: the tag exists. It uses map::at, which
// throws std::out_of_range (and terminates under -fno-exceptions, e.g. the
// Switch build). Entity has no "none" value, so this one cannot be softened
// into a silent miss the way GetEntitiesByGroup was — guard it.
if (registry.DoesTagExist("player")) {
    Entity p = registry.GetEntityByTag("player");
}

// GetEntitiesByGroup returns an empty vector on a miss (and logs, throttled),
// so no guard is required. DoesGroupExist still exists if you want to branch.
for (auto &e : registry.GetEntitiesByGroup("enemies")) { ... }
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

**Important:** `GameState` transitively pulls in `ecs.h`, `assetStore.h`,
`logger.h`, `tilemapLoader.h`, every component in `common/components/` and every
system in `common/systems/`. It does **not** include `common/input/`
(`touchControls.h`, `virtualGamepad.h`) or `common/net/`; states using touch, the
virtual gamepad, or networking must include those themselves.

Do not *rely* on that transitive reach — include what you use. The breadth of
this header is a documented defect (`KNOWN_ISSUES.md` §8: ~713 headers and ~145k
preprocessed lines to declare a 23-line interface) and trimming it is a v3 goal,
so code leaning on the transitive path breaks when it is fixed. Listing your own
includes costs nothing and makes that upgrade a no-op.

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
- **There is no `pause()` hook** — `pushState` does not call anything on the
  state beneath it (it is simply no longer the `back()` that gets ticked), and
  it does not `onExit()` it. Only `popState` calls `resume()`, on the newly
  exposed top; `changeState` never calls `resume()`. `popState` on an empty
  stack is a safe no-op.
- **Initialize in `onEnter()`, tear down in `onExit()`** — not the ctor/dtor.
  `pushState` calls `onEnter()` *after* pushing; `changeState` calls it *before*
  pushing (so during a `changeState`'s `onEnter()` the new state is not yet on
  the stack — `getGameStates().back()` is not you). `clean()` calls `onExit()`
  on every stacked state before deleting.
- **`clean()` exits the whole stack** — it calls `onExit()` on every stacked
  state, top-down in reverse push order, then deletes them all and sweeps the
  defunct list. A pushed-under state was entered, so it owes an `onExit()`;
  `clean()` on an empty stack is a safe no-op.
- **The machine owns every state pointer** — pass `new`-allocated states and
  never delete them yourself.
- **`clean()` is not automatic** — `~GameStateMachine()` is empty and frees
  nothing. Call `gameStateMachine.clean()` yourself during shutdown (the
  examples do it in `Game::Destroy()`), or every stacked and defunct state
  leaks.

---

## Game Loop Pattern

The `Game` class owns the window, renderer, and main loop. It delegates to
the state machine. **The engine ships no Game class** — the game writes it.

Timestep is variable dt with a 60 FPS **cap**: each state computes
`MILLISECS_PER_FRAME - elapsed` and `SDL_Delay`s the remainder. Nothing
enforces a minimum frame rate. Games typically stack two throttles:
`SDL_RENDERER_PRESENTVSYNC` *and* the state's own delay budget.

There is **no keyboard or physical-gamepad abstraction** — nothing under
`common/` references `SDL_Keycode`, `SDL_SCANCODE`, `SDL_GameController` or
`SDL_Joystick`. `common/input/` holds the touch primitives
(`touchControls.h`: `TouchZone`/`TouchPoint`/`TouchZones`/`TouchInput`) and the
SDL-free virtual gamepad (`virtualGamepad.h`: `MakeVPadLayout(w, h, style =
VPadStyle::Xbox)`, `EvalVPad`, `VPadState`, `VPadLayout`, `enum class VPadStyle
{ Xbox, Snes }`). Keyboard/quit handling is raw `SDL_PollEvent` inside each
state's `processInput()`.

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

The `millisecondsPreviousFrame` used above is a protected `int` on `GameState`
(alongside `m_loadingComplete`, `m_exiting` and `m_textureIDList`) — inherit it,
don't redeclare it. `GameState`'s constructor is protected and its destructor is
virtual.

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

Re-adding an existing id destroys the previous texture, so any `SDL_Texture *`
you cached from an earlier `GetTexture` for that id dangles. A failed load logs
via `Logger::Err` and returns — the id simply stays unmapped, and `GetTexture`
keeps returning `nullptr`.

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
                    renderer, assetStore.get(), &logger);  // AssetStore*, not AssetStore_Ptr
```

```cpp
XmlLoader loader("assets/game.xml");
if (!loader.IsValid()) { /* file missing or malformed — getters return {} */ }
```
`GetTextures`/`GetObjects` also return an empty vector for an unknown stateId or
a state with no `<TEXTURES>`/`<OBJECTS>` child — silently, with no log.
`LoadTexturesFromXml` does the `IsValid()` check for you and logs on failure.

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
| `NetVarIntPack` / `NetVarIntUnpack` | Free functions (there is no `NetVarInt` class) for variable-length ints; unpack rejects non-canonical encodings |
| `NetSocket` | Non-blocking UDP socket + `NetNowMs()` / `NetAddressToString()` helpers |

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
- **Parsing is strict by design.** `NetVarIntUnpack` rejects non-canonical
  encodings, and `NetMessageReader::ReadString` fails unless the wire string
  carries its own terminator — hand-rolled packets that "decode fine" elsewhere
  are rejected here. Anything a peer gets to see must come from `NetNonce32()`
  (ChaCha20 keystream), never `NetRandom32()` (raw xorshift64, invertible from
  a handful of outputs).
- **Every vital `Send` is its own datagram** — broadcasting N reliable messages
  per tick costs N datagrams per client.
- **Overflowing the unacked-vital window kills the connection** (96 entries or
  16 KB), it does not block or drop.
  A vital chunk is also rejected — same fatal `SetError` path — when the ring
  write wraps into bytes still pinned by a live unacked entry, so a connection
  can die with the 16 KB pool far from full if acks are lagging.
- **`NetSnapshot` is two-phase:** `AddItem` only before `Finish()`,
  `FindItem`/`GetItemByIndex` only after. An empty delta base must still have
  `Finish()` called on it.
  This applies to every base, not just empty ones: `NumItems()` still reports
  items on an unfinished snapshot while `GetItemByIndex`/`FindItem` bail out
  early, so `Create()` iterates and reads uninitialised type/id locals.
- **`Apply()` wants the exact delta size.** Pass the byte count `Create()`
  returned, never `sizeof(buf)` — trailing bytes fail the parse and `Apply()`
  returns false with `to` left reset.
- **`NetSnapshotDelta::Create()` returns 0 for "no changes" and -1 for "buffer
  too small".** 0 is not an error — it means send nothing this tick. Size the
  buffer with `EstimateSize()`.
- **Zero coupling to the ECS or the engine tick.** Snapshots are flat arrays;
  the game hand-marshals ECS components in and out. Nothing in `net/` drives a
  tick — each net example paces itself.
- **Hard ceilings:** chunk ≤ 1200 bytes, datagram ≤ 1400, snapshot ≤ 256 items /
  2048 int32s, prediction cache = 16 ticks (~267 ms at 60 Hz).
- **`-fno-exceptions` on Switch** — `Registry::GetSystem` (`common/ecs.h:389`)
  and `Registry::GetEntityByTag` (`common/ecs.cpp:288`) still throw through
  `std::map::at`; on Switch (`examples/nx-platformer/Makefile:30`) those become
  `abort`, not catchable errors — guard with `HasSystem()` / `DoesTagExist()`.
  `GetEntitiesByGroup` no longer throws: a group nobody joined logs once and
  returns an empty vector (`common/ecs.cpp:323-336`).

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

D-pad uses 8-way angle sectors (diagonals set two flags), with an inner
deadzone at 25% of the radius and an outer cutoff at the radius; a component
registers when it exceeds `tan(22.5°)` of the other.

---

## Platforms

| Platform | How it works |
|----------|-------------|
| **Linux** | `.deb` package or build from source via `Makefile.debian` |
| **Nintendo Switch** | devkitPro + SDL2 portlibs, compiles engine into `.nro` |
| **Android** | Gradle + CMake + NDK, engine compiled into JNI library via `SDLActivity` |
| **Windows** | MinGW-w64 cross-compile from Linux: `make -f Makefile.win` builds `build/win/libstormenginev2.dll` + `tests.exe` against the *same* vendored SDL2 sources Android uses (`vendor/android/`). `make -f Makefile.win test` runs the suite under Wine. Not covered by CI. |
| **WSL** | WSL2 with the same `apt` prerequisites drives the Linux `Makefile.debian` path |

When consuming as a submodule (Switch, Android, or game-specific), the engine
is compiled directly into the game binary — no shared library to distribute.

---

## Build System

### Engine (installed library)

**There is no plain `Makefile` at the repo root.** Bare `make` fails — every
root invocation needs `-f Makefile.debian`.

(Inside `Dockerfile.debian`'s image, `Makefile.debian` is copied in as
`/opt/library/Makefile`, so bare `make target` / `make install` works there —
which is why `.github/scripts/ci-build-examples.sh` uses them unqualified.)

```bash
make -f Makefile.debian              # default: build tests -> run tests -> build .so (no clean)
make -f Makefile.debian target       # build ONLY ./bin/libstormenginev2.so
make -f Makefile.debian test         # build ./bin/tests and run it
make -f Makefile.debian test-target  # build tests without running
make -f Makefile.debian run-test     # rebuild tests if stale, then run them
make -f Makefile.debian clean        # rm ./bin/* and every *.o AND *.d under repo root
sudo make -f Makefile.debian install # .so + headers to /usr/local
```

The default goal **runs the whole spec suite before building the library** —
a failing spec aborts the `.so` build. Use `test` for iteration, `target` when
you only want the library.

`install` has **no prerequisites** — it will happily install a stale `.so`.
Build `target` first.

`install` honours `DESTDIR` and `PREFIX` (`make install DESTDIR=<staging>`),
which is how the release workflow stages the `.deb` tree — same code path as a
from-source install, so the two cannot drift. It `rm -rf`s
`$(PREFIX)/include/stormengine2` before copying, so headers deleted from
`common/` stop living on forever, then strips `*.o`/`*.d`/`*.cpp` out of the
copied tree.

**Build profiles.** `PROFILE=debug` (default) is `-O0 -g`; `PROFILE=release`
is `-O2` with no `-g`. `OPT` and `DEBUGFLAGS` can also be set on their own
(`make OPT=-O1`, `make DEBUGFLAGS=-ggdb3`). The release workflow passes
`PROFILE=release`, so the shipped `.deb` is no longer an `-O0` build.

```bash
make -f Makefile.debian PROFILE=release target
```

### Engine (Windows, cross-compiled)

```bash
make -f Makefile.win deps      # one-time: cross-build vendored SDL2 et al
make -f Makefile.win           # build/win/libstormenginev2.dll + tests.exe
make -f Makefile.win test      # run the spec suite under wine64
make -f Makefile.win clean     # drop objects, keep deps
make -f Makefile.win distclean # drop everything including deps
```

Prereqs: `sudo apt install mingw-w64 cmake`. Uses `x86_64-w64-mingw32-g++-posix`
(the win32-threads gcc has no `<thread>`, which `specs/net/netLoopback.spec.cpp`
needs). Deps come from `vendor/android/`, so Windows and Android can never be on
different SDL versions. `stage-dlls` resolves the import graph and copies the
mingw runtime DLLs beside the binaries — a missing one is a silent non-zero exit
under Wine, not an error message. Examples cross-build through
`examples/examples.win.mk`, though no example ships a `Makefile.win` yet.

### Build System Hazards

- **`clean` has repo-wide blast radius.** `ROOT_DIR` derives from `base.mk`'s
  own realpath, so `cd examples/puzzle && make clean` deletes every `*.o` in
  the repository. Since `examples.mk` is `all: clean $(TARGET)`, building any
  one example wipes every other example's objects.
- **`ROOT_DIR` is `:=`, deliberately.** `MAKEFILE_LIST` grows as make reads the
  generated `.d` files, so with a recursive `=` the `lastword` became whichever
  `.d` was read last: `-I$(ROOT_DIR)/vendor` pointed at nothing, `clean` walked
  a subtree instead of the repo, and the `%.o` rule stopped matching in
  `editor/` and `examples/` — make silently fell through to its built-in
  flagless `g++ -c`, dropping `-std=c++17` and every `-I`. Do not change it.
- **Header dependency tracking is on (desktop).** `base.mk` compiles with
  `-MMD -MP` and the generated `.d` files are `-include`d, so editing a header
  rebuilds every object that includes it. `Makefile.debian`'s `all` no longer
  starts with `clean`. `examples/examples.mk` and `editor/Makefile` still do
  (`all: clean $(TARGET)`), so building any example or the editor is still a
  full rebuild — and still has repo-wide blast radius (see above).
- **Command-line flags are stamped.** `PROFILE`/`OPT`/`DEBUGFLAGS` arrive on the
  command line and touch no file, so neither the `.d` files nor the `base.mk`
  prerequisite would notice them changing. `base.mk` md5s `CCFLAGS` into
  `$(ROOT_DIR)/.build-flags` and every `%.o` depends on that stamp, so
  `make && make PROFILE=release target` can no longer link `-O0 -g` objects
  into the release `.so`.
- **The Switch build lives in `examples/nx-platformer/`.** A dead `Makefile.nx`
  at the repo root used to shadow it; it was deleted (P42) because it recursed
  into a root `Makefile` that does not exist.
- **`cd editor && make` launches the editor** — the link rule executes the
  binary. Examples do *not* auto-launch.
- **GTK3 and Lua are linked unconditionally**, even for headless networking
  examples. `pkg-config gtk+-3.0` must resolve or nothing compiles.

### CI

`pr-validate.yml` builds `Dockerfile.debian`, runs the spec suite, then pipes
`.github/scripts/ci-build-examples.sh` into the image: it installs the engine
and builds **every desktop example** (jrpg, netchat, netplay-checkers, netrepl,
platformer, puzzle, shooter, sports, strategy) plus the editor. The editor is
compile-only — it is the one tree that calls `NFD_*`, and libnfd has no Debian
package, so the link step cannot run. `nx-platformer` and `android-platformer`
are not built at all: neither toolchain is in the image, and `.dockerignore`
keeps both trees out of the build context. The Windows cross-build is not in CI
either.

The script overrides `LIB` on the command line to drop `-lnfd` and GTK; no
example calls either.

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

Two patterns exist, and the desktop examples are *not* the submodule one.
`examples/platformer/Makefile` is three lines that `include ../examples.mk`,
and `examples.mk` links the **installed** library (`-lstormenginev2`) — it
compiles no engine source at all. The submodule pattern, compiling the engine
into the game binary, is what `examples/nx-platformer/Makefile`,
`examples/android-platformer/app/jni/CMakeLists.txt` and
`examples/examples.win.mk` do. When copying it, glob engine sources
**recursively**.

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
- NFD (`-lnfd`, editor-only — `Makefile.debian` filters it back out of the
  library and test links, and CI overrides `LIB` for the examples; `vendor/nfd`
  ships only `nfd.h` and a LICENSE, so nothing can actually link it on Debian)
- zlib (`-lz`)

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
11. **No throw on a game data path** — component ids are range-checked before any `bitset` access (`set`/`test` carry an `out_of_range` throw that would be emitted into a `-fno-exceptions` game TU, e.g. the Switch build), a miss returns a default/`nullptr` instead of aborting, and every diagnostic is throttled to its first 4 occurrences per call site (`ECS_MAX_DIAGNOSTIC_REPORTS`). `GetEntitiesByGroup` returns an empty vector on a miss; `AssetStore::GetTexture` returns `nullptr`. The two reachable throws left on a data path are `TileMapLoader`'s `std::stoi` and `GetEntityByTag`'s `.at()`.

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

### Reading a Component That May Not Be There

```cpp
// GetComponent returns a reference, so a miss cannot be reported: it logs
// (throttled) and hands back a per-thread fallback that two misses SHARE.
if (auto *rb = player.TryGetComponent<RigidBodyComponent>()) {
    rb->velocity.x = 40.0;
}
// Registry-side equivalent: registry.TryGetComponent<RigidBodyComponent>(player)
// Liveness / lookup guards: registry.IsAlive(e), registry.DoesTagExist("player")
// before registry.GetEntityByTag("player") — that one still .at()s.
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
| **Collision** — simplified shapes | `TileMapLoader` parses `hasCollider` + `colliderW`/`colliderH` into each `Tile` and creates **no entities at all** — its whole output is `const Map &getMap()`. The game iterates `getMap()` and adds `BoxColliderComponent` itself; `CollisionSystem` then does AABB. |
| **Animated tiles** — editor-authored | Not supported at runtime. The editor writes animation fields into `.map` files and `TileMapLoader` parses and discards them, because `Tile` has nowhere to put them (fixing that changes `sizeof(Tile)`, an ABI break). Drive tile animation from game code with `AnimationComponent`. |

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
- **Fixed timestep for consistency:** the engine does no frame pacing at all — `common/states/gameState.h` defines `FPS`/`MILLISECS_PER_FRAME` and nothing else; there is not a single `SDL_Delay` in `common/`. Every state re-implements the variable-dt loop with a 60 FPS `SDL_Delay` budget itself, so a game wanting a deterministic fixed timestep simply writes a different loop. (This is the same point as Key Design Decision 6, "no main loop".)
- **Layers for filtering:** use entity groups (`registry.GroupEntity`) to partition entities for collision logic. Note: one group per entity and one tag per entity. `GroupEntity` calls `RemoveEntityGroup` first, so re-grouping *moves* an entity rather than adding a second membership; groups are not a bitmask layer system.

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

The `android-platformer` variant is not a pure port: it is the reference consumer of the engine's virtual gamepad. It builds the layout once from the logical window size (`MakeVPadLayout(w, h)` — Xbox lettering by default), feeds SDL touches through `EvalVPad` each frame, letterboxes with `SDL_RenderSetLogicalSize`, and handles orientation by overriding `setOrientationBis` in `PlatformerActivity` (it requests `SCREEN_ORIENTATION_FULL_SENSOR`, so the game follows the device through all four orientations even with the auto-rotate lock on; SDL overwrites the manifest's `screenOrientation` from native code, so the manifest alone cannot decide this). It also links `common/net/` (`GLOB_RECURSE` + the `INTERNET` permission).

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
| Lean on `gameState.h`'s transitive includes instead of including what you use | It is true that `gameState.h` drags in SDL2 and every component/system — ~713 headers, ~145k preprocessed lines, to declare a 23-line interface — but that path is a documented defect (KNOWN_ISSUES #8) and goes away in v3. Include what you use in your own headers. |
| Move `AssetStore_Ptr` to multiple states | Move once to first state, pass raw ptr/ref after |
| Delete states inline on transition | Use the state machine's push/pop/change (deferred deletion) |
| Add components before registering systems | Register systems first, then create entities |
| `AddComponent` on a live entity to get it into a system | Kill and recreate the entity — membership is computed once |
| Call bare `make` at repo root — there is no default `Makefile` | Name the makefile: `-f Makefile.debian` for the Linux `.so` + spec suite, `-f Makefile.win` for the MinGW-w64 cross-build (`build/win/libstormenginev2.dll`, `tests.exe` under Wine) |
| Run `./bin/tests` from outside the repo root | Run from repo root — specs hardcode relative paths |
| Forget `Update()` before `Poll()` in networking | `NetConnection` caches the clock in `Update` only |
| Keep `NetChunk::data` past the callback | Copy it — the next `Feed()` overwrites the scratch buffer |
| Call `install` without building first | Build `target` first — `install` has no prerequisites |
| Use separate textures when a sprite sheet would do | Use `SpriteComponent.srcRect` to pick regions from a sheet |
| Use complex collision shapes for simple objects | Use `BoxColliderComponent` (AABB) — add custom shapes only when needed |
| Jittery camera | Smooth camera following with interpolation in your state's `update()` |
| Mix pixel-perfect and physics-based collision | Pick one approach per game |
| Forget to null-check `AssetStore::GetTexture` | It returns `nullptr` for missing IDs, not an exception |
| Call `GetComponent<T>` where a miss is possible | Use `TryGetComponent<T>` — `GetComponent` returns a shared per-thread fallback on a miss, and two misses alias each other |
| Call `GetEntityByTag` unguarded | Guard with `DoesTagExist(tag)` — it still `.at()`s, which terminates under `-fno-exceptions` |
| Keep an `Entity` past the frame it might die in | Ids are recycled and `Entity` carries no generation, so a stale handle can kill a live entity; `IsAlive` cannot tell them apart. Re-look up by tag or group |
| Hand-build an `Entity(88)` and call methods on it | Every forwarder now null-checks `registry` and no-ops with a throttled log — it is not UB any more, but it still does nothing |

---

## When to Use

Use this skill when working on any project that consumes Storm! Engine v2 —
whether as an installed library, a git submodule, or by compiling `common/`
directly. This includes writing game states, custom components/systems,
tilemap-based levels, networking code, or touch input for mobile targets.

## Naming Conventions

Member-naming schemes coexist and are load-bearing:
- Engine ECS core: bare names (`numEntities`)
- `GameStateMachine`: `m_` prefix (`m_gameStates`)
- Engine `common/net/` **and** game/state code: trailing underscore (`nowMs_`, `sock_`, `renderer_`)
- Editor (`editor/src/`): bare `m` prefix, no underscore (`mMousePosX`, `mSpriteComponent`)

Method casing is PascalCase everywhere except the state machine and the
`GameState` virtuals — that includes `common/net/` (`Start`, `Update`, `Poll`,
`Send`) and `common/input/` (`MakeVPadLayout`, `EvalVPad`). The camelCase
exceptions are `pushState`, `processInput`, `onEnter` and friends — overrides
must match camelCase.

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
  only. Games needing complex physics implement their own. It does not merely
  *detect*: on an overlap it calls `Kill()` on each entity that has a
  `RigidBodyComponent` (static scenery survives). There is no callback, no event
  queue, no event bus — `common/systems/collision.h:32` carries the
  `// TODO: emit an event`. Any game needing bounce, damage, triggers or pickups
  hand-rolls its own overlap pass.
- **Thirty-two component types, process-wide.** `MAX_COMPONENTS` is 32 and
  `Signature` is `std::bitset<32>`; ids come from one global counter, so the cap
  is per binary, not per `Registry`. Overflow no longer throws — the id is
  range-checked, logged and ignored — but a system whose `RequireComponent<T>`
  was dropped keeps an empty signature, and an empty signature matches **every**
  entity. Prefer widening a component with a `kind` enum over declaring a new one.
- No built-in scene editor beyond the tile map editor. Entity placement is
  code-driven or XML-driven.
- The engine ships no main loop, no Game class, no window management.
- No keyboard or *physical* gamepad abstraction — games read SDL (or libnx
  `PadState` on Switch) themselves. `common/input/` ships SDL-free touch
  primitives (`touchControls.h`) plus the on-screen virtual gamepad
  (`virtualGamepad.h`): `MakeVPadLayout(w, h, VPadStyle = VPadStyle::Xbox)` /
  `EvalVPad`, lettered Xbox-style by default (Y top, X left, B right, A bottom)
  or `VPadStyle::Snes` on request.
- `common/net/` is absent from the **Switch** build only:
  `examples/nx-platformer/Makefile` globs `$(wildcard $(dir)/*.cpp)` over
  `include/stormengine2` (a symlink to `common/`), which is non-recursive and
  picks up 6 of the 13 translation units. The **Android** example does build it
  — `app/jni/CMakeLists.txt` uses `GLOB_RECURSE` and the manifest carries
  `INTERNET`.
- The editor's shadowing copy of `common/components/sprite.h` is gone;
  `editor/include/` was deleted and the editor now compiles against the
  installed engine headers with `#include <stormengine2/components/sprite.h>`.
- Ten further defects are **real, understood and deliberately unfixed in 1.x**
  because each needs a source or ABI break; they are tracked in
  `KNOWN_ISSUES.md` with a workaround apiece. Highlights: recycled entity ids
  with no generation counter, implicit `Entity(std::size_t)` conversion,
  component set frozen at admission, copyable `NetServer`/`NetClient`, tile
  animation fields discarded by the loader, and **no namespaces — every engine
  type (`Entity`, `Registry`, `Logger`, `Tile`…) is a global symbol**, so a game
  declaring its own collides. (`docs/TECH_DEBT.md` is gitignored and local-only;
  `KNOWN_ISSUES.md` is the tracked record.)


