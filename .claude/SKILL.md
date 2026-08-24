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

> **Current release: v1.3.0** - public API stable for the 1.x line.
> Repo: `github.com/WillSams/storm-engine-v2` · License: WTFPL
>
> Since v1.2.1: v1.2.2 added the safe accessors (`TryGetComponent`, `IsAlive`,
> `DoesTagExist`), `VPadStyle`, and the MinGW-w64 cross-build; v1.2.4 changed
> the `kNetControlClose` wire format (see **Networking Rules**) and made
> `TileMapLoader` report failures; v1.2.5 was documentation only; v1.2.6 fixed
> the build only - no API change. v1.3.0 added `ContactSystem` (and deprecated
> `CollisionSystem`), `Text`, a physical `Gamepad`, `AssetStore` font and sound
> caches, `GameState::CapFrameRate()`, a `pkg-config` file and an installed
> starter game at `$(PREFIX)/share/stormengine2/template`. `CHANGELOG.md` is
> authoritative.
>
> **v1.3.0 is a deliberate one-off ABI break: upgrading needs a REBUILD, not
> just a relink.** `sizeof(AssetStore)` went from 112 to 208 bytes to hold the
> font and sound maps, and every game allocates the store in its own code
> (`std::make_unique<AssetStore>()`), so a binary compiled against 1.2.x headers
> allocates 112 bytes and then runs a 1.3.0 constructor that initialises out to
> 208. Nothing warns. Never swap `libstormenginev2.so` underneath an
> already-built game.

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
| **Asset Store** | `<stormengine2/assetStore.h>` | Texture, font and sound loading and caching by string ID |
| **Tilemap Loader** | `<stormengine2/tilemapLoader.h>` | Load `.map` files (editor or CSV format) |
| **XML Loader** | `<stormengine2/xmlLoader.h>` | Parse XML asset/entity definitions via tinyxml2 |
| **Logger** | `<stormengine2/logger.h>` | Timestamped, color-coded logging with callback hooks |
| **Text** | `<stormengine2/text.h>` | Header-only one-line SDL_ttf drawing: `Draw` / `DrawCentred` / `Measure`. Null-safe, leak-free, opens no font |
| **Networking** | `<stormengine2/net/net.h>` | UDP host/join: reliable chunks, snapshots, kick/ban |
| **Gamepad** | `<stormengine2/input/gamepad.h>` | One physical `SDL_GameController`: `Down`/`Pressed`/`Released`, deadzone-rescaled sticks. Not the same file as `virtualGamepad.h` |
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
registry.AddSystem<ContactSystem>();  // CollisionSystem is deprecated in 1.3.0

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
| `SpriteComponent` | `components/sprite.h` | `assetId`, `width`, `height`, `zIndex`, `isFixed`, `flip`, `srcRect`, `offset` | **`width`/`height` are the source rect** — the ctor builds `srcRect{srcRectX, srcRectY, width, height}`, so they must match the sheet cell; resize with `TransformComponent.scale`, not by passing on-screen dimensions. `srcRectX` defaults to `0`, so a transparent cell 0 makes a sprite silently invisible.
| `AnimationComponent` | `components/animation.h` | ctor args: `numFrames`, `frameSpeedRate`, `vertical` (default `true`), `isLooped` (default `true`), `frameOffset`; also public: `currentFrame`, `startTime` (set to `SDL_GetTicks()` in the ctor), `lastFrame` (non-looped stop frame, 0 = use `numFrames - 1`) |
| `BoxColliderComponent` | `components/boxCollider.h` | `width`, `height`, `offset` (glm::vec2) |

### Built-in Systems

| System | Requires | What it does |
|--------|----------|-------------|
| `MovementSystem` | Transform + RigidBody | Moves entities by `velocity * deltaTime` |
| `RenderSystem` | Transform + Sprite | Draws sprites sorted by `zIndex`; applies the camera offset except to sprites with `isFixed` (HUD/screen-space), and honours `sprite.offset`, `transform.scale`, `transform.rotation` and `sprite.flip` |
| `AnimationSystem` | Sprite + Animation | Advances sprite sheet frames. **`vertical == true` (the default) advances `srcRect.y`; `false` advances `srcRect.x`.** Mismatching the flag against the sheet's layout is silent — the sprite samples outside the texture and draws nothing, or sits on frame 0. `examples/platformer`'s `rabbit.png` is a vertical strip (37x1026) and passes `true`; the scaffold's sheet is horizontal and passes `false`. |
| `ContactSystem` | Transform + BoxCollider | **Preferred since 1.3.0.** Reports AABB overlaps with a normal and a penetration depth, plus begin/end callbacks and a pair filter. Never kills, moves or writes anything |
| `CollisionSystem` | Transform + BoxCollider | AABB collision detection; kills entities with RigidBody on contact. **Deprecated in 1.3.0** - behaviour byte-identical, kept for source compatibility with 1.0-1.2 games |
| `RenderColliderSystem` | Transform + BoxCollider | Debug overlay: draws collider outlines in green |

### Contacts

`ContactSystem` (`<stormengine2/systems/contact.h>`) detects overlaps and
*reports* them. It never kills an entity, never moves one and never writes a
component, because the engine has no system scheduler - what a contact means,
and in what order to respond, is the game's call.

```cpp
registry_.AddSystem<ContactSystem>();
auto &contacts = registry_.GetSystem<ContactSystem>();

// Skip pairs you never care about. This is where layers, masks and sensors
// live, and it costs no component slot. The filter runs BEFORE the manifold is
// built, which is what keeps a dense volley cheap.
contacts.SetPairFilter([](const Entity &a, const Entity &b) {
    return !(a.BelongsToGroup("bullets") && b.BelongsToGroup("bullets"));
});

// Once per pair on the transition, not once per frame while touching.
contacts.SetOnBeginContact([](const Contact &c) { /* c.normal, c.depth */ });
contacts.SetOnEndContact([](const Entity &a, const Entity &b) { /* ... */ });

// In update(), after registry_.Update():
contacts.Update();
for (const auto &c : contacts.GetContacts()) {
    // c.a always holds the LOWER entity id, so c.normal is a stable unit axis
    // pointing a -> b along the axis of least penetration, and c.depth > 0.
    c.a.GetComponent<TransformComponent>().position -=
        ContactSystem::MinimumTranslation(c);
}
```

Rules to know before you use it:

- **`GetContacts()` is sorted by `(a.id, b.id)`** and is invalidated by the next
  `Update()`. `Update()` is safe to call twice in a frame: the second call
  reports the same contacts and fires nothing new.
- **`Overlaps` is strict - a shared edge is not a contact**, because a
  zero-area overlap has no meaningful normal. `CollisionSystem::isCollision` is
  inclusive and deliberately still is, so the two disagree on touching boxes.
- **`SetOnEndContact` silently drops a pair whose entity died.**
  `Registry::Update()` returns a killed id to the free list in the same pass
  that removes it from the system, so handing it back would be KNOWN_ISSUES #1.
- **Membership is still computed once.** Adding a `BoxColliderComponent` to a
  live entity never gets it into `ContactSystem`; create the entity with its
  collider.
- **`BoundsOf`, `Overlaps`, `Manifold` and `MinimumTranslation` are static** -
  usable with no system registered. `CollisionSystem` now calls `BoundsOf` too,
  so there is exactly one copy of the bounds math.
- **The broadphase sweeps X only.** Everything stacked in one column degrades to
  all-pairs. Fine for a puck and six boards, or bullets and enemies.
- **Do not build tilemap collision out of one collider entity per tile.** The
  manifold picks the axis of least penetration *per box*, so a character sliding
  along adjacent tile colliders picks up a sideways normal from a tile it barely
  overlaps and is shoved out of the wall - the ghost-vertex problem. Snap to a
  solid grid instead (see the platformer block below).

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
system in `common/systems/` (`ContactSystem` included, since `collision.h`
includes `contact.h`). It does **not** include `common/input/`
(`touchControls.h`, `virtualGamepad.h`, `gamepad.h`), `common/text.h` or
`common/net/`; states using touch, either gamepad, `Text`, or networking must
include those themselves.

Do not *rely* on that transitive reach — include what you use. The breadth of
this header is a documented defect (`KNOWN_ISSUES.md` §8: ~713 headers and ~145k
preprocessed lines to declare a 23-line interface) and trimming it is a v3 goal,
so code leaning on the transitive path breaks when it is fixed. Listing your own
includes costs nothing and makes that upgrade a no-op.

**If the game does not use the ECS, include
`<stormengine2/states/gameStateBase.h>` instead** (1.3.0+). Same `GameState`
interface, none of the extras: 80,265 preprocessed lines against `gameState.h`'s
146,748, a 45% saving per TU. `gameStateMachine.h` is on the base header too, so
including both stays slim — that was deliberate, since leaving the machine on
the convenience header would have handed the whole engine back and made the slim
path pointless. `gameState.h` includes the base, so the two cannot drift and a
game can switch either way at any time.

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

Timestep is variable dt with a 60 FPS **cap**. Since 1.3.0 that pacing lives in
`GameState::CapFrameRate()` instead of being retyped in every state: it sleeps
out the rest of `MILLISECS_PER_FRAME`, returns the elapsed seconds, and rolls
`millisecondsPreviousFrame` forward. Nothing enforces a minimum frame rate.
Games typically stack two throttles: `SDL_RENDERER_PRESENTVSYNC` *and* the
state's own delay budget.

There is **no keyboard abstraction** - nothing under `common/` references
`SDL_Keycode` or `SDL_SCANCODE`, so keyboard/quit handling is raw
`SDL_PollEvent` inside each state's `processInput()`. `common/input/` holds
three separate things: the touch primitives (`touchControls.h`:
`TouchZone`/`TouchPoint`/`TouchZones`/`TouchInput`), the SDL-free on-screen
virtual gamepad (`virtualGamepad.h`: `MakeVPadLayout(w, h, style =
VPadStyle::Xbox)`, `EvalVPad`, `VPadState`, `VPadLayout`, `enum class VPadStyle
{ Xbox, Snes }`), and, since 1.3.0, a real controller wrapper (`gamepad.h`:
`Gamepad` over `SDL_GameController`).

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

### Frame Pacing in States

Since 1.3.0 the pacing lives on `GameState` itself. `CapFrameRate` is protected,
non-virtual and adds no member, so it changes neither the layout nor the vtable:

```cpp
void PlayState::update() {
    // Sleeps out the rest of the 60 FPS budget, rolls
    // millisecondsPreviousFrame forward, and returns how long the frame took
    // in seconds. The default clamps a hitch to 0.05 s so one long frame (a
    // level load, a breakpoint, a window drag) cannot teleport everything
    // through a wall. Pass 0 for the raw, unclamped delta.
    const double deltaTime = CapFrameRate();

    registry_.Update();  // flush deferred adds/kills FIRST
    registry_.GetSystem<MovementSystem>().Update(deltaTime);
    registry_.GetSystem<AnimationSystem>().Update();
    registry_.GetSystem<ContactSystem>().Update();
}
```

`millisecondsPreviousFrame` is a protected `int` on `GameState` (alongside
`m_loadingComplete`, `m_exiting` and `m_textureIDList`) - inherit it, don't
redeclare it. Shadowing it with a `millisecondsPreviousFrame_` of your own is
exactly what `CapFrameRate` exists to stop: seven in-repo states had written the
budget out by hand and five of them carried that shadow member, and all five
have dropped it. `GameState`'s constructor is protected and its destructor is
virtual.

`MILLISECS_PER_FRAME` is defined as `1000 / 60` (targeting 60 FPS) in
`gameState.h`, and `CapFrameRate` is written against it. Hand-rolling the same
loop still works if you want a different budget.

---

## Asset Store

Textures, fonts and sounds are loaded and cached by string ID. Every getter
returns `nullptr` for a missing id (none of them throw) - null-check the result.

```cpp
assetStore->AddTexture(renderer, "player", "./assets/gfx/player.png");
SDL_Texture *tex = assetStore->GetTexture("player");

// A TTF_Font is rasterised at ONE point size, so register one id per size you
// draw at ("hud-18", "title-32"). Requires TTF_Init() to have been called.
assetStore->AddFont("hud-18", "./assets/fonts/font.ttf", 18);
TTF_Font *font = assetStore->GetFont("hud-18");

// Requires Mix_OpenAudio() to have been called.
assetStore->AddSound("jump", "./assets/sfx/jump.wav");
Mix_PlayChannel(-1, assetStore->GetSound("jump"), 0);

assetStore->ClearAssets();  // frees textures, fonts AND sounds (also in dtor)
```

**Call `ClearAssets()` before `TTF_Quit()`, `Mix_CloseAudio()` or `SDL_Quit()`.**
Those calls free every open font and chunk themselves, so a store destroyed
afterwards hands already-freed pointers to `TTF_CloseFont` / `Mix_FreeChunk`.
The store is usually owned by the `Game` and outlives the state that shut the
subsystems down, which is exactly the order that goes wrong. The store does not
initialise SDL_ttf or SDL_mixer: a game that never calls `TTF_Init()` or
`Mix_OpenAudio()` gets a logged failure and a `nullptr`, not a crash.

Re-adding an existing id destroys the previous asset, so any `SDL_Texture *`,
`TTF_Font *` or `Mix_Chunk *` you cached from an earlier getter for that id
dangles. A failed load logs via `Logger::Err` and returns - the id simply stays
unmapped, and the getter keeps returning `nullptr`.

`sizeof(AssetStore)` went from 112 to 208 bytes in 1.3.0 to hold the two new
maps. That is a deliberate one-off 1.x ABI break: rebuild consuming games rather
than relinking them.

The `AssetStore_Ptr` (`std::unique_ptr<AssetStore>`) is typically created in
`Game` and moved into the first state via `std::move`. Pass raw pointers or
references to subsequent states.

### Text

`<stormengine2/text.h>` is the five-call SDL_ttf dance (render a surface, make a
texture, query its size, copy it, free both) written once. Header-only and free
of engine types, so it costs nothing to a TU that does not include it and it
reaches every platform target. It is null-safe: a null renderer or font, or an
empty string, draws nothing and returns `{0, 0}` - which is exactly what
`GetFont` hands back for an unregistered id. It never opens or closes a font,
and never leaks the surface or texture on any path, including the failure ones.

```cpp
#include <stormengine2/text.h>

TTF_Font *font = assetStore_->GetFont("hud-18");
Text::Draw(renderer_, font, "Score: 400", 10, 10, SDL_Color{255, 255, 255, 255});
Text::DrawCentred(renderer_, font, "GAME OVER", windowWidth_ / 2, 200,
                  SDL_Color{255, 210, 50, 255});
SDL_Point size = Text::Measure(font, "Score: 400");  // layout without drawing
```

`DrawCentred` replaces the hand-guessed `windowWidth / 2 - 30` offsets, which
drift the moment the string or the point size changes. Both drawing calls create
and destroy a texture per call, so cache the result yourself for a long string
that is on screen every frame.

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

**The constructor cannot fail loudly — check the map before using it.** Since
v1.2.4 every failure (missing file, unreadable, no tiles parsed) is reported
through `Logger::Err`, but the object still constructs and `getMap()` returns
an empty `Map`. A successfully loaded empty map and a failed load are
indistinguishable from the return value alone, so a game that skips the check
renders a blank level with no crash and no obvious cause.

```cpp
TileMapLoader loader("assets/tilemaps/level.map", "", 32);
if (loader.getMap().empty()) {
    // Logger::Err already said why. Bail here rather than rendering nothing.
    return false;
}
```

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
- **A clean disconnect is authenticated (v1.2.4+).** `kNetControlClose` carries
  the sender's cookie pair ahead of the reason string; a close that does not
  quote the right nonces is ignored. Before v1.2.4 a source-address match alone
  was enough, so one spoofed datagram could kick any connected player. Slots
  that are not yet online stay exempt — they hold no game state and have no
  server nonce to quote. **This is a wire-format change:** a pre-1.2.3 peer's
  clean disconnect is not parsed by a current server, and the connection is
  instead reaped by the ~10 s timeout.
- **Repeated `CONNECT_READY` is answered with a fresh ACCEPT (v1.2.4+).**
  ACCEPT is the last datagram of the handshake and nothing acknowledges it, so
  a single lost ACCEPT used to strand the client re-sending into silence while
  the server had already counted it connected and fired `onConnect_`. Do not
  build join logic that assumes ACCEPT arrives exactly once.
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

## Physical Gamepad

`<stormengine2/input/gamepad.h>` wraps one real `SDL_GameController`. This is
**not** `virtualGamepad.h`, which is the SDL-free on-screen touch pad above.
Hold a `Gamepad` by value in your `Game` and pass a pointer to the states that
need it.

```cpp
#include <stormengine2/input/gamepad.h>

Gamepad pad;                 // in Game, by value (non-copyable: it holds a handle)
pad.OpenFirstAttached();     // SDL does not always emit CONTROLLERDEVICEADDED
                             // for a pad that was already plugged in

// In processInput(), feed it device add/remove events:
while (SDL_PollEvent(&event)) { pad.HandleEvent(event); /* ... */ }

// Then sample it once, after the event loop. Polled, not accumulated, so a pad
// unplugged mid-hold cannot latch a direction on.
pad.Update();

if (pad.Down(GamepadButton::Right)) { /* held this frame */ }
if (pad.Pressed(GamepadButton::A))  { /* only the frame it went down */ }
if (pad.Released(GamepadButton::A)) { /* only the frame it came up */ }

const float x = pad.Current().leftX;          // -1..1, deadzone removed
const float t = pad.Current().triggerRight;   // 0..1
```

- **`Down()` is true for the d-pad *or* the left stick past half travel**, so a
  game binds a direction once and either input drives it. Read
  `Current().leftX` when you want the analog value instead.
- **Call `Shutdown()` before `SDL_Quit()` / `SDL_QuitSubSystem()`.**
  `SDL_GameControllerQuit` force-closes and frees every open controller, so a
  `Gamepad` destroyed afterwards calls `SDL_GameControllerClose` on freed
  memory. The destructor is then a harmless second call.
- **The deadzone is radial and rescaled**, not a threshold: `SetDeadzone()`
  overrides the 8000 default (~24% of the `Sint16` range). Thresholding alone
  makes the stick jump to a quarter speed the instant it crosses; the rescale
  gives a continuous ramp from a standstill, and being radial rather than
  per-axis keeps the reachable set a circle.
- **`GamepadState`, `GamepadDown`, `GamepadPressed`, `GamepadReleased` and
  `GamepadNormaliseStick` are SDL-free and pure**, so the edge detection and
  deadzone maths are spec'd with no device attached.
- Only the first pad is taken; a second one plugging in is ignored rather than
  stealing control mid-game. `Connected()` and `Name()` report what is open.

`examples/shooter`, `examples/strategy` and `examples/sports` are the in-repo
consumers; the first two deleted their own diverging copies of this file for it.

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
- **GTK3 is linked unconditionally**, even for headless networking examples, so
  `pkg-config gtk+-3.0` must resolve or nothing compiles. `-llua` was dropped
  from `base.mk`: nothing in `common/`, `specs/` or `editor/` includes a Lua
  header or references a `lua_`/`luaL_` symbol.

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

The script still overrides `LIB` on the command line to drop GTK, which no
example calls. `-lnfd` no longer needs stripping: `base.mk` keeps it in a
separate `EDITOR_LIB` that only `editor/Makefile` puts on a link line.

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
- GTK3 (linked unconditionally on desktop, even for headless examples)
- NFD (`-lnfd`, editor-only - `base.mk` keeps it out of the shared `LIB` and in
  `EDITOR_LIB`, which only `editor/Makefile` uses; `vendor/nfd` ships only
  `nfd.h` and a LICENSE, so nothing can actually link it on Debian)
- zlib (`-lz`)

---

## Key Design Decisions

1. **ECS over inheritance** — entities are IDs, components are plain data, systems are logic. No deep inheritance hierarchies.
2. **Deferred entity lifecycle** — creation and destruction are batched and flushed on `registry.Update()` to avoid invalidating iterators mid-loop.
3. **System membership is computed once** - `AddComponent`/`RemoveComponent` only flip bits; they never re-evaluate system membership. Kill-and-recreate is the usual fix; the manual alternative is the public `RemoveEntityFromSystems` + `AddEntityToSystems` pair (see ECS Traps 1).
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

> **A non-looping animation never reports itself finished, and never removes its
> entity.** `AnimationSystem` clamps it with
> `currentFrame = min(max(frame, 0), min(lastFrame, numFrames - 1))`, so
> `currentFrame` **cannot reach `numFrames`** — the obvious
> `currentFrame >= numFrames` test is dead code and the effect lives forever.
> Test `currentFrame >= numFrames - 1` and kill the entity yourself:
>
> ```cpp
> // Explosions, hit sparks, anything one-shot. Without this the entity count
> // climbs for the lifetime of the state.
> auto finished = [](Entity &e) {
>     const auto *a = e.TryGetComponent<AnimationComponent>();
>     return !a || a->currentFrame >= a->numFrames - 1;
> };
> for (auto &e : effects) { if (finished(e)) e.Kill(); }
> effects.erase(std::remove_if(effects.begin(), effects.end(), finished),
>               effects.end());
> ```

### Tilemap Design

| Principle | Storm Engine v2 implementation |
|-----------|-------------------------------|
| **Tile size** — 16x16, 32x32, 64x64 | `TileMapLoader` constructor takes `tileSize` (default 32). The JRPG example uses 8 to preserve exact editor pixel coordinates. |
| **Auto-tiling** — use for terrain | Not built in. The tile editor is manual paint/erase. Auto-tiling is a game-side concern. |
| **Collision** - simplified shapes | `TileMapLoader` parses `hasCollider` + `colliderW`/`colliderH` into each `Tile` and creates **no entities at all** - its whole output is `const Map &getMap()`. The game iterates `getMap()` and decides. Prefer a solid-grid array plus per-axis snapping over one collider entity per tile: `ContactSystem`'s per-box manifold catches on the seams between adjacent tiles, and `CollisionSystem` would kill the player outright. |
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

- **Pixel-perfect vs physics-based:** pick one approach per game. `ContactSystem` gives you detection plus a manifold (unit normal along the axis of least penetration, and the depth on that axis); the response is yours to write. The deprecated `CollisionSystem` kills entities with `RigidBodyComponent` on contact - simple arcade collision, not a physics solver, and not one you can steer.
- **Fixed timestep for consistency:** the engine paces a frame but does not schedule one. `GameState::CapFrameRate()` is the only `SDL_Delay` in `common/` (`common/states/gameState.h`): it sleeps out the 60 FPS budget defined by `FPS`/`MILLISECS_PER_FRAME` and hands back a variable, hitch-clamped dt. There is still no fixed-step accumulator, so a game wanting a deterministic timestep writes its own loop on top. (Related to Key Design Decision 6, "no main loop".)
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
- **Tile collision** - resolve against a solid-tile grid by hand. `ContactSystem` reports overlaps but never resolves one, and its per-box manifold catches on seams between adjacent tiles; `CollisionSystem` kills on contact, which is fatal here

All four are game-side state on the `PlayState`, not engine features. Members:

```cpp
bool   onGround_       = false;
Uint32 leftGroundAt_   = 0;   // SDL ticks when we last left the ground
Uint32 jumpBufferedAt_ = 0;   // SDL ticks of the last unconsumed jump press
bool   jumpHeld_       = false;

static constexpr Uint32 COYOTE_MS = 100;  // forgive a late press after an edge
static constexpr Uint32 BUFFER_MS = 120;  // forgive an early press before landing
static constexpr float  JUMP_V    = -520.0f;
static constexpr float  GRAVITY   =  1400.0f;
```

Coyote time and jump buffering are the same trick in opposite directions —
each remembers a timestamp and asks whether it is still recent:

```cpp
// processInput(): record the press, do not act on it here
case SDL_KEYDOWN:
    if (e.key.keysym.sym == SDLK_SPACE && !e.key.repeat) {
        jumpBufferedAt_ = SDL_GetTicks();
        jumpHeld_ = true;
    }
    break;
case SDL_KEYUP:
    if (e.key.keysym.sym == SDLK_SPACE) jumpHeld_ = false;
    break;

// update(), after registry_.Update() and before the movement system runs
auto &rb = player_->GetComponent<RigidBodyComponent>();
const Uint32 now = SDL_GetTicks();

const bool canCoyote  = onGround_ || (now - leftGroundAt_)   <= COYOTE_MS;
const bool wantsJump  = jumpBufferedAt_ && (now - jumpBufferedAt_) <= BUFFER_MS;

if (wantsJump && canCoyote) {
    rb.velocity.y   = JUMP_V;
    jumpBufferedAt_ = 0;      // consume it, or one press fires every frame
    onGround_       = false;
    leftGroundAt_   = 0;      // and do not let coyote fire a second jump
}

// Variable height: cutting velocity on release gives a short hop, holding
// gives the full arc. Only ever shorten an ascent.
if (!jumpHeld_ && rb.velocity.y < 0.0f) {
    rb.velocity.y *= 0.5f;
}

rb.velocity.y += GRAVITY * static_cast<float>(deltaTime);
```

`MovementSystem` integrates `velocity * deltaTime` and nothing else — there is
no gravity, no ground, and no resolution in the engine. `CollisionSystem`
detects AABB overlap and **kills** entities carrying a `RigidBodyComponent`,
which is fatal for a platformer, so do not register it. `ContactSystem` is safe
to register (it only reports), but do not build the tile layer out of one
collider entity per tile: the manifold is per box, so a player sliding along a
floor picks up a sideways normal from the next tile along and gets shoved out of
the wall. Keep `ContactSystem` for the handful of distinct bodies (pickups,
hazards, triggers) and resolve the grid yourself, one axis at a time. Resolving
both at once lets a corner push the player sideways off a flat floor:

```cpp
// solidGrid_[row][col] mirrors the tilemap; TILE_PX is tileSize * scale.
bool PlayState::IsSolid(int col, int row) const {
    if (row < 0 || row >= (int)solidGrid_.size())    return false;
    if (col < 0 || col >= (int)solidGrid_[row].size()) return false;
    return solidGrid_[row][col];
}

void PlayState::ResolvePlayer(float dt) {
    auto &tf = player_->GetComponent<TransformComponent>();
    auto &rb = player_->GetComponent<RigidBodyComponent>();

    // --- X axis ---
    tf.position.x += rb.velocity.x * dt;
    int top    = (int)(tf.position.y) / TILE_PX;
    int bottom = (int)(tf.position.y + PLAYER_H - 1) / TILE_PX;
    if (rb.velocity.x > 0.0f) {
        int side = (int)(tf.position.x + PLAYER_W - 1) / TILE_PX;
        for (int r = top; r <= bottom; ++r)
            if (IsSolid(side, r)) {
                tf.position.x = side * TILE_PX - PLAYER_W;
                rb.velocity.x = 0.0f;
                break;
            }
    } else if (rb.velocity.x < 0.0f) {
        int side = (int)(tf.position.x) / TILE_PX;
        for (int r = top; r <= bottom; ++r)
            if (IsSolid(side, r)) {
                tf.position.x = (side + 1) * TILE_PX;
                rb.velocity.x = 0.0f;
                break;
            }
    }

    // --- Y axis ---
    tf.position.y += rb.velocity.y * dt;
    int left  = (int)(tf.position.x) / TILE_PX;
    int right = (int)(tf.position.x + PLAYER_W - 1) / TILE_PX;
    const bool wasOnGround = onGround_;
    onGround_ = false;
    if (rb.velocity.y > 0.0f) {
        int foot = (int)(tf.position.y + PLAYER_H - 1) / TILE_PX;
        for (int c = left; c <= right; ++c)
            if (IsSolid(c, foot)) {
                tf.position.y = foot * TILE_PX - PLAYER_H;
                rb.velocity.y = 0.0f;
                onGround_ = true;
                break;
            }
    } else if (rb.velocity.y < 0.0f) {
        int head = (int)(tf.position.y) / TILE_PX;
        for (int c = left; c <= right; ++c)
            if (IsSolid(c, head)) {
                tf.position.y = (head + 1) * TILE_PX;
                rb.velocity.y = 0.0f;   // bonk: kill upward velocity
                break;
            }
    }
    if (wasOnGround && !onGround_) leftGroundAt_ = SDL_GetTicks();  // start coyote
}
```

Because the player is moved by hand here, do **not** also register
`MovementSystem` for it, or the position integrates twice.

The engine's `platformer` example demonstrates the basic pattern: `TransformComponent` + `RigidBodyComponent` + `SpriteComponent` + `AnimationComponent` + `BoxColliderComponent`. The `nx-platformer` and `android-platformer` variants show the same game on Switch and Android.

The `android-platformer` variant is not a pure port: it is the reference consumer of the engine's virtual gamepad. It builds the layout once from the logical window size (`MakeVPadLayout(w, h)` — Xbox lettering by default), feeds SDL touches through `EvalVPad` each frame, letterboxes with `SDL_RenderSetLogicalSize`, and handles orientation by overriding `setOrientationBis` in `PlatformerActivity` (it requests `SCREEN_ORIENTATION_FULL_SENSOR`, so the game follows the device through all four orientations even with the auto-rotate lock on; SDL overwrites the manifest's `screenOrientation` from native code, so the manifest alone cannot decide this). It also links `common/net/` (`GLOB_RECURSE` + the `INTERNET` permission).

#### Shooter (Side-scrolling shoot-em-up)

- **Bullet spawning** — create entities on input, add `RigidBodyComponent` with fixed velocity, `Kill()` when off-screen
- **Periodic enemy waves** — spawn entities on a timer, use tags/groups to distinguish factions
- **Scrolling background layers** — multiple `SpriteComponent` entities at different `zIndex` values, scroll at different rates for parallax (game-side)
- **Collision as gameplay** - register `ContactSystem` and act on `GetContacts()`; a bullet-vs-enemy hit needs a score bump and an explosion, not just two deaths

`CollisionSystem` looks like a fit here ("one hit = death") and is not: it kills
**both** entities, so the player dies on any enemy touch, and it gives you no
hook to score the kill or spawn the explosion. `examples/shooter` moved to
`ContactSystem` with a pair filter:

```cpp
registry_.AddSystem<ContactSystem>();
registry_.GetSystem<ContactSystem>().SetPairFilter(
    [](const Entity &a, const Entity &b) {
        // Only bullet-vs-enemy and player-vs-enemy mean anything here.
        const bool aEnemy = a.BelongsToGroup("enemies");
        const bool bEnemy = b.BelongsToGroup("enemies");
        if (aEnemy == bEnemy) return false;   // enemy-vs-enemy, or neither
        const Entity &other = aEnemy ? b : a;
        return other.BelongsToGroup("bullets") || other.HasTag("player");
    });
```

Rejecting in the filter rather than in your own loop is what keeps a dense
volley cheap: without it every bullet pairs with every other bullet, and the
manifold for each of those is built and then thrown away. Two more things to
respect when you walk `GetContacts()`: a killed entity stays in the system's
vector until the next `registry_.Update()`, so track what has died by id or a
second bullet scores the same enemy twice; and contacts arrive sorted by entity
id, so give bullets their own pass ahead of the player's rather than letting the
two interleave.

```cpp
void PlayState::SpawnBullet() {
    Entity b = registry_.CreateEntity();
    b.Group("bullets");
    auto &tf = player_->GetComponent<TransformComponent>();
    b.AddComponent<TransformComponent>(tf.position + glm::vec2(24.0f, 8.0f),
                                       glm::vec2(1.0f, 1.0f), 0.0);
    b.AddComponent<RigidBodyComponent>(glm::vec2(600.0f, 0.0f));
    b.AddComponent<SpriteComponent>("bullet", 8, 8, 2);
    b.AddComponent<BoxColliderComponent>(8, 8);
    // The entity is NOT live until the next registry_.Update(). Do not read it
    // back this frame.
}

void PlayState::SpawnWave(Uint32 now) {
    if (now - lastWaveAt_ < WAVE_INTERVAL_MS) return;
    lastWaveAt_ = now;
    for (int i = 0; i < 4; ++i) {
        Entity e = registry_.CreateEntity();
        e.Group("enemies");
        e.AddComponent<TransformComponent>(
            glm::vec2(windowWidth_ + 32.0f, 60.0f + i * 90.0f),
            glm::vec2(1.0f, 1.0f), 0.0);
        e.AddComponent<RigidBodyComponent>(glm::vec2(-90.0f, 0.0f));
        e.AddComponent<SpriteComponent>("enemy", 32, 32, 2);
        e.AddComponent<BoxColliderComponent>(32, 32);
    }
}
```

Culling off-screen entities is mandatory, not housekeeping: nothing reaps them,
component storage is indexed by entity id, and memory per component type is
O(highest id ever used). A shooter that never kills its bullets grows every
pool forever.

```cpp
void PlayState::CullOffscreen() {
    // DoesGroupExist first -- GetEntitiesByGroup on an unknown group returns an
    // empty vector on v1.2.2+, but aborts on older builds.
    if (!registry_.DoesGroupExist("bullets")) return;
    // Copy the vector: Kill() mutates the group while you are iterating it.
    auto bullets = registry_.GetEntitiesByGroup("bullets");
    for (auto &b : bullets) {
        const auto &tf = b.GetComponent<TransformComponent>();
        if (tf.position.x > windowWidth_ + 64.0f) b.Kill();  // deferred
    }
}
```

Parallax is game-side. Scroll each layer by its own factor and wrap:

```cpp
void PlayState::ScrollBackground(float dt) {
    static const float speed[3] = { 12.0f, 40.0f, 110.0f };  // far -> near
    for (int i = 0; i < 3; ++i) {
        auto &tf = layers_[i].GetComponent<TransformComponent>();
        tf.position.x -= speed[i] * dt;
        if (tf.position.x <= -windowWidth_) tf.position.x += windowWidth_;
    }
}
```

The engine's `shooter` example (Alien Attack) demonstrates this pattern.

#### Puzzle (Grid-based / falling blocks)

- **Grid logic is entirely game-side** — the engine has no grid abstraction; represent the board as a 2D array in your state
- **Entity reuse** — the `puzzle` example reuses a pool of block entities rather than creating/destroying each frame, avoiding `registry.Update()` churn
- **Custom components for game state** — e.g., `CellComponent` with grid coordinates, `ShapeComponent` for tetromino identity
- **Text for the HUD** - score, level, next-piece preview. Cache the font once with `AssetStore::AddFont` and draw with `Text::Draw`/`Text::DrawCentred`; the `puzzle` example does exactly that and no longer opens a font by hand
- **No physics needed** - blocks snap to grid; `RigidBodyComponent`, `ContactSystem` and `CollisionSystem` are all typically unused

The board is a plain array — the ECS holds only what is *drawn*. Keeping the
rules out of the ECS is what makes a puzzle game testable:

```cpp
static constexpr int COLS = 10, ROWS = 20, CELL = 24;

// 0 = empty, otherwise a colour/shape id. This, not the registry, is the game.
std::array<std::array<int, COLS>, ROWS> board_{};

bool PlayState::Fits(const Piece &p, int atCol, int atRow) const {
    for (const auto &c : p.cells) {          // cells are offsets from the origin
        const int col = atCol + c.x, row = atRow + c.y;
        if (col < 0 || col >= COLS || row >= ROWS) return false;
        if (row >= 0 && board_[row][col] != 0)     return false;  // row<0 = above ceiling
    }
    return true;
}

int PlayState::ClearFullRows() {
    int cleared = 0;
    for (int row = ROWS - 1; row >= 0; --row) {
        bool full = true;
        for (int col = 0; col < COLS; ++col)
            if (board_[row][col] == 0) { full = false; break; }
        if (!full) continue;
        for (int r = row; r > 0; --r) board_[r] = board_[r - 1];   // collapse down
        board_[0].fill(0);
        ++cleared;
        ++row;                       // re-test this row -- it holds new contents
    }
    return cleared;
}
```

**Reuse a fixed entity pool.** A tetromino lands roughly once a second and each
one is 4 cells; creating and killing entities per lock burns entity ids forever
(storage is indexed by id, so pools grow to the highest id ever issued) and
adds `registry_.Update()` churn. Allocate `COLS * ROWS` entities once and move
them:

```cpp
bool PlayState::onEnter() {
    registry_.AddSystem<RenderSystem>();
    cells_.reserve(COLS * ROWS);
    for (int i = 0; i < COLS * ROWS; ++i) {
        Entity e = registry_.CreateEntity();
        e.AddComponent<TransformComponent>(glm::vec2(-CELL, -CELL),   // parked offscreen
                                           glm::vec2(1.0f, 1.0f), 0.0);
        e.AddComponent<SpriteComponent>("blocks", CELL, CELL, 1);
        cells_.push_back(e);
    }
    registry_.Update();      // one flush: every cell joins RenderSystem here
    return true;
}

// Each frame, park every cell then place only the occupied ones. No entity is
// ever created or killed, so system membership never needs recomputing.
void PlayState::SyncBoardToEntities() {
    for (auto &e : cells_)
        e.GetComponent<TransformComponent>().position = glm::vec2(-CELL, -CELL);

    std::size_t next = 0;
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col) {
            if (board_[row][col] == 0 || next >= cells_.size()) continue;
            auto &e = cells_[next++];
            e.GetComponent<TransformComponent>().position =
                glm::vec2(boardX_ + col * CELL, boardY_ + row * CELL);
            // Pick the colour cell out of the sheet by hand -- srcRect is only
            // driven by AnimationSystem, which these entities do not use.
            e.GetComponent<SpriteComponent>().srcRect.x =
                (board_[row][col] - 1) * CELL;
        }
}
```

The engine's `puzzle` example (Storm Tetris) demonstrates custom ECS components, entity reuse, and SDL_ttf rendering.

#### JRPG (Tile-based RPG)

- **Tile-based world** — `TileMapLoader` with small tile size (the `jrpg` example uses 8px to preserve exact editor coordinates)
- **NPC interaction** — game-side proximity check against tagged entities, trigger dialogue state
- **Typewriter dialogue** - game-side character-by-character reveal, drawn with `Text::Draw` over a font cached in the `AssetStore`
- **State transitions** — push a `DialogueState` over the `PlayState` for conversations; pop when done
- **No real-time physics** — movement is grid-based or tile-based, not velocity-driven

NPCs and the player carry game-side components — the engine ships neither. The
`jrpg` example's are worth copying verbatim:

```cpp
enum class Direction { Up, Down, Left, Right };

struct PlayerComponent {
    float     moveSpeed   = 120.0f;
    Direction facing      = Direction::Down;
    bool      isMoving    = false;
    int       walkFrame   = 0;        // 0-3 within the current direction's run
    float     animTimer   = 0.0f;
    float     animInterval = 0.15f;   // seconds per walk frame
};

struct NpcComponent {
    std::string name;
    std::string dialogue;
    Direction   facing       = Direction::Down;
    float       interactDist = 48.0f;  // pixels
};
```

Both must be **default-constructible** — component pools are dense
`std::vector<T>` indexed by entity id, so the engine value-initialises slots
you never touched. Note these two alone spend 2 of your 32 process-wide
component ids.

Interaction is a proximity scan over a group, not a contact query: you want the
*closest* NPC inside a radius, which neither collision system reports. Do not
reach for `CollisionSystem` here - it kills on contact:

```cpp
// Returns the closest NPC in range, or nullopt. Pure query, no side effects.
std::optional<Entity> PlayState::NpcInRange() const {
    if (!registry_.DoesGroupExist("npcs")) return std::nullopt;

    const auto &ptf = player_->GetComponent<TransformComponent>();
    std::optional<Entity> best;
    float bestDist = 0.0f;

    for (auto &npc : registry_.GetEntitiesByGroup("npcs")) {
        const auto &ntf = npc.GetComponent<TransformComponent>();
        const auto &n   = npc.GetComponent<NpcComponent>();
        const float d   = glm::distance(ptf.position, ntf.position);
        if (d > n.interactDist) continue;
        if (!best || d < bestDist) { best = npc; bestDist = d; }
    }
    return best;
}
```

Dialogue is a **pushed** state, not a flag — that is what freezes the world for
free, since `GameStateMachine` only forwards to the top of the stack:

```cpp
void PlayState::processInput() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE) {
            if (auto npc = NpcInRange()) {
                const auto &n = npc->GetComponent<NpcComponent>();
                gameStateMachine_->pushState(
                    new DialogueState(renderer_, font_, n.name, n.dialogue,
                                      gameStateMachine_));
                return;   // MUST return: pushState ran onEnter on the new top
            }
        }
    }
}
```

`PlayState` is not deleted by a push — it stays on the stack and `resume()` is
called when the dialogue pops. Do not re-run `onEnter()` there; re-initialising
would rebuild the world and leak the old entities.

Typewriter reveal is a character count driven by elapsed time. Reveal-all on a
second press is what players expect:

```cpp
void DialogueState::update() {
    const Uint32 now = SDL_GetTicks();
    revealed_ = std::min<std::size_t>(
        text_.size(), (now - startedAt_) / MS_PER_CHAR);
}

void DialogueState::processInput() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type != SDL_KEYDOWN || e.key.keysym.sym != SDLK_SPACE) continue;
        if (revealed_ < text_.size()) {
            revealed_ = text_.size();          // first press: reveal all
        } else {
            gameStateMachine_->popState();     // second press: dismiss
            return;                            // this state is now defunct
        }
    }
}

void DialogueState::render() {
    // SDL_ttf renders a whole string; substr to the revealed prefix. Text::Draw
    // absorbs the rest: it returns {0, 0} rather than blitting at length 0 (a
    // hand-rolled version has to skip it, because TTF_Render* returns nullptr
    // on an empty string and SDL_CreateTextureFromSurface(nullptr) crashes),
    // and it never leaks the intermediate surface or texture on any path.
    Text::Draw(renderer_, assetStore_->GetFont("dialogue-18"),
               text_.substr(0, revealed_), boxX_, boxY_, colour_);
    // Still a create/destroy per frame -- cache the texture yourself if the
    // string is long or the box is always up.
}
```

The engine's `jrpg` example demonstrates tile-based world loading, NPC interaction, and typewriter dialogue.

#### Sports (Top-down action)

- **Custom AI components** — player decision-making, positioning tables, reaction logic (e.g., the `sports` example's hockey AI)
- **Puck/ball physics** - custom component for the game object with velocity, friction, bounce - the engine's `RigidBodyComponent` + `MovementSystem` handle basic velocity, but the bounce is yours. Make the walls ordinary collider entities, then push the puck out by `contact.depth` and mirror it about the normal `ContactSystem` reported: `puck.velocity = glm::reflect(puck.velocity, normal)`. `Contact::normal` points `a -> b` and `a` is always the lower entity id, so negate it on the frames where your object is `b`. `examples/sports` does exactly this, which retired four hardcoded axis-aligned bounce cases and the rink-edge constants behind them
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

Define the wire protocol as an enum first. Both sides read the same first int,
so a message the peer does not know is skipped rather than misparsed:

```cpp
enum : int32_t {
    kMsgJoinRequest = 1,   // client -> host
    kMsgSeatAssign  = 2,   // host -> client: your colour
    kMsgMove        = 3,   // client -> host: proposed move
    kMsgFullState   = 4,   // host -> all: authoritative board + turn
    kMsgReject      = 5,   // host -> one client: illegal move, resync
};
```

**The host is the only authority.** A client never applies its own move —
it proposes, and waits for the state that comes back. This is what makes
cheating and desync impossible rather than merely unlikely:

```cpp
void Match::SendMove(int fromCell, int toCell) {
    NetMessageWriter w;
    w.WriteInt(kMsgMove);
    w.WriteInt(fromCell);
    w.WriteInt(toCell);
    client_.Send(w.Data(), w.Size(), /*vital=*/true);   // moves must not drop
    // Deliberately no local board mutation here.
}

void Match::OnHostChunk(int clientId, const NetChunk &chunk) {
    NetMessageReader r(chunk.data, chunk.size);
    int32_t msg = 0;
    if (!r.ReadInt(msg)) return;            // every Read* is checked

    if (msg == kMsgMove) {
        int32_t from = 0, to = 0;
        if (!r.ReadInt(from) || !r.ReadInt(to)) return;
        if (seatOf_[clientId] != turn_ || !IsLegal(from, to)) {
            NetMessageWriter rej;
            rej.WriteInt(kMsgReject);
            server_.Send(clientId, rej.Data(), rej.Size(), true);
            BroadcastFullState();           // resync the liar
            return;
        }
        ApplyMove(from, to);
        turn_ = (turn_ == kRed) ? kBlack : kRed;
        BroadcastFullState();
    }
}
```

Full-state broadcast is the cheapest correct approach for turn-based play. It
also solves late joiners for free — a new client's first state message *is* the
whole game:

```cpp
void Match::BroadcastFullState() {
    NetMessageWriter w;
    w.WriteInt(kMsgFullState);
    w.WriteInt(turn_);
    w.WriteInt(static_cast<int32_t>(board_.size()));
    for (int32_t cell : board_) w.WriteInt(cell);
    // A vital Send is one datagram per client. At one broadcast per turn that
    // is free; do not reach for this shape at 60 Hz.
    server_.Broadcast(w.Data(), w.Size(), /*vital=*/true);
}

void Match::OnClientChunk(const NetChunk &chunk) {
    NetMessageReader r(chunk.data, chunk.size);
    int32_t msg = 0;
    if (!r.ReadInt(msg) || msg != kMsgFullState) return;

    int32_t turn = 0, count = 0;
    if (!r.ReadInt(turn) || !r.ReadInt(count)) return;
    if (count < 0 || count > kMaxCells) return;   // never trust a peer's length

    std::vector<int32_t> next(static_cast<std::size_t>(count));
    for (auto &cell : next)
        if (!r.ReadInt(cell)) return;             // partial read: drop it whole

    turn_  = turn;
    board_ = std::move(next);
    SyncBoardToEntities();                        // ECS mirrors state, never owns it
}
```

Four rules this shape depends on, each of which bites otherwise:

- **`chunk.data` points into per-connection scratch** that the next `Feed()`
  overwrites. `NetMessageReader` consumes it inside the callback, which is safe.
  Storing the pointer for later is not — copy the bytes if you must defer.
- **Call `Update()` before `Poll()` every frame**, on both sides.
  `NetConnection` caches its clock in `Update(nowMs)`; a Poll-only loop reads
  RTT 0 and never times anyone out.
- **Callbacks fire synchronously** on the thread calling `Poll()`. There is no
  threading in the module, so no locking is needed — and no work may block.
- **Seat on `CONNECT_READY`, not on `CONNECT`.** Since v1.2.4 a repeated
  `CONNECT_READY` is answered with a fresh ACCEPT, so `onConnect_` can fire for
  a client you have already seated. Make seating idempotent — key it on
  `clientId` and ignore a second call, or you consume the seat the real second
  player needed.

The engine's `netplay-checkers` example demonstrates graphical, authoritative-netplay checkers with full-state sync.

---

## Anti-Patterns

| Don't | Do |
|-------|-----|
| Call `SDL_PollEvent` in both Game and State | Let the active state own all event polling |
| Forget `registry.Update()` before systems | Always flush deferred adds/kills first |
| Lean on `gameState.h`'s transitive includes instead of including what you use | It is true that `gameState.h` drags in SDL2 and every component/system — ~713 headers, ~145k preprocessed lines, to declare a 23-line interface — but that path is a documented defect (KNOWN_ISSUES #8) and goes away in v3. Include what you use in your own headers. On 1.3.0+, a game that does not use the ECS should include `states/gameStateBase.h` instead: same interface, 80,265 lines, 45% less. |
| Move `AssetStore_Ptr` to multiple states | Move once to first state, pass raw ptr/ref after |
| Delete states inline on transition | Use the state machine's push/pop/change (deferred deletion) |
| Add components before registering systems | Register systems first, then create entities |
| `AddComponent` on a live entity to get it into a system | Kill and recreate the entity — membership is computed once |
| Register `CollisionSystem` in new code | Register `ContactSystem`: it reports overlaps with a normal and depth and touches nothing. `CollisionSystem` is deprecated in 1.3.0 and can only respond by killing both movable entities |
| Build tilemap collision from one collider entity per tile | Keep a solid-grid array and resolve per axis; a per-box manifold catches on the seam between adjacent tiles |
| Retype the `SDL_Delay` budget, or shadow `millisecondsPreviousFrame` with your own member | Call `CapFrameRate()` at the top of `update()`: protected, non-virtual, returns the dt and rolls the timestamp forward |
| Hand-roll the `TTF_Render*` / `CreateTextureFromSurface` / free dance, or re-open a font per draw | Cache the font with `AssetStore::AddFont` and draw with `Text::Draw` / `Text::DrawCentred` |
| Destroy the `AssetStore` after `TTF_Quit()` / `Mix_CloseAudio()` / `SDL_Quit()` | Call `ClearAssets()` first; those calls have already freed every open font and chunk |
| Let a `Gamepad` outlive `SDL_Quit()` / `SDL_QuitSubSystem()` | Call `Shutdown()` first; `SDL_GameControllerQuit` has already freed the controller |
| Relink a 1.2.x game against the 1.3.0 `.so` | Rebuild it: `sizeof(AssetStore)` changed from 112 to 208 and the game allocates the store in its own code |
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

## References

Load these when the task calls for them rather than reading them up front.

| File | Use it when |
|------|-------------|
| `references/new-game-scaffold/` | Starting a new standalone game. A complete compiling project: real Makefile (the in-repo examples' 2-line Makefile does **not** work outside `examples/`), the `Game` class and main loop the engine does not ship, and a `PlayState` demonstrating system-registration order, deferred flush, input ownership and render. Compiles against any 1.x install. Read its `README.md` first. (Since 1.3.0 the engine also *installs* a starter game at `$(PREFIX)/share/stormengine2/template`, built with `pkg-config`; that one targets 1.3.0 and uses `ContactSystem`, `AddFont` and `Text`.) |
| `references/eval/` | Asking whether a model can actually build a game from this skill. Three task specs, a scoring harness (`run-eval.sh`), and recorded results. Read `eval/README.md` for the method; the failures it surfaces are what should become new rules here. |
| `references/compile-errors.md` | A build fails. Real compiler and linker output mapped to cause and fix, including the stale-install signatures (`no member named 'DoesTagExist'`, `RenderSystem::Update` arity) and the runtime failures that look like build problems. |

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
- The engine does not play audio - `AssetStore::AddSound`/`GetSound` cache and
  free `Mix_Chunk`s, but `Mix_OpenAudio`, `Mix_PlayChannel` and music are the
  game's job, and nothing in `common/` calls them.
- No built-in physics engine - AABB only, and no solver. Since 1.3.0
  `ContactSystem` *detects* and reports (unit normal, penetration depth,
  once-per-pair begin/end transitions) and never touches an entity, so bounce,
  damage, triggers and pickups are all writable, but every response is still
  written by the game, and `MinimumTranslation` is a value it hands you rather
  than something it applies. The deprecated `CollisionSystem` is the opposite
  trade: it acts and cannot be observed, calling `Kill()` on each entity that
  has a `RigidBodyComponent` (static scenery survives). There is still no event
  bus and no event queue (`KNOWN_ISSUES.md` #10), and the broadphase sweeps one
  axis.
- **Thirty-two component types, process-wide.** `MAX_COMPONENTS` is 32 and
  `Signature` is `std::bitset<32>`; ids come from one global counter, so the cap
  is per binary, not per `Registry`. Overflow no longer throws — the id is
  range-checked, logged and ignored — but a system whose `RequireComponent<T>`
  was dropped keeps an empty signature, and an empty signature matches **every**
  entity. Prefer widening a component with a `kind` enum over declaring a new one.
- No built-in scene editor beyond the tile map editor. Entity placement is
  code-driven or XML-driven.
- The engine ships no main loop, no Game class, no window management.
- No keyboard abstraction - games read SDL (or libnx `PadState` on Switch)
  themselves. `common/input/` ships SDL-free touch primitives
  (`touchControls.h`), the on-screen virtual gamepad (`virtualGamepad.h`:
  `MakeVPadLayout(w, h, VPadStyle = VPadStyle::Xbox)` / `EvalVPad`, lettered
  Xbox-style by default with Y top, X left, B right and A bottom, or
  `VPadStyle::Snes` on request), and since 1.3.0 a real controller wrapper
  (`gamepad.h`: `Gamepad`), which covers one pad at a time and still no
  keyboard.
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


