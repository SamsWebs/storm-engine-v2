# Storm Engine v2 - Project Reference Document

## Overview

**Storm Engine v2** is a modern, ECS-based game engine written in C++ with a focus on simplicity, correctness, and cross-platform support. It provides a clean abstraction over SDL2 for rendering, input handling, audio, and asset management, while maintaining a robust Entity-Component-System architecture for game logic.

### Key Features
- **ECS Architecture**: Clean separation of entities, components, systems, and pools with automatic memory management
- **Cross-Platform**: Linux desktop, Windows (MinGW-w64 cross-compile), Android, Nintendo Switch homebrew
- **Asset Management**: Cached textures (SDL_image), fonts (SDL_ttf) and sounds (SDL_mixer), XML-based asset configuration
- **Game State Machine**: Robust state management with deferred deletion to prevent use-after-free bugs
- **Input Handling**: Keyboard/mouse, a `Gamepad` wrapper over SDL_GameController, touch controls for mobile
- **Rendering**: Full SDL2 integration with custom RenderSystem for efficient sprite rendering, plus header-only `Text` for one-line SDL_ttf drawing
- **Contact Detection**: AABB `ContactSystem` reports overlaps as `{a, b, normal, depth}` and acts on none of them; a pair filter carries layers, masks and sensors

---

## Architecture Summary

### Core Components (from TUTORIAL.md)

#### ECS Foundation
```cpp
// Entity: a 16-byte handle (id + registry back-pointer). Only
// Registry::CreateEntity() produces a valid one.
Entity player = registry.CreateEntity();

// Component: data attached to an entity
player.AddComponent<TransformComponent>(glm::vec2(100, 200));
player.AddComponent<SpriteComponent>("player-sprite", 64, 64, 1);
player.AddComponent<BoxColliderComponent>(64, 64);

// System: a class that declares the components an entity must have
class MoveSystem : public System {
public:
  MoveSystem() { RequireComponent<TransformComponent>(); }
  void Update(double dt) {
    for (auto &entity : GetSystemEntities()) { /* ... */ }
  }
};
registry.AddSystem<MoveSystem>();
```

#### Asset Management (AssetStore)
- **Texture Loading**: `IMG_Load` + `SDL_CreateTextureFromSurface` with error handling
- **Font and Sound Caches** (v1.3+): `AddFont(id, path, ptSize)` / `GetFont(id)`, `AddSound(id, path)` / `GetSound(id)`. A font is rasterised at one point size, so one id per size
- **XML Parsing**: `XmlLoader` for asset configuration files
- **Null-safe Getters**: every getter returns `nullptr` for a missing id rather than throwing, so callers null-check and nothing aborts under the Switch build's `-fno-exceptions`
- **Teardown Order**: `ClearAssets()` frees textures, fonts and sounds, and must run *before* `TTF_Quit()` / `Mix_CloseAudio()` / `SDL_Quit()` - those calls free the same handles themselves

#### Game State Machine
- **State Ownership**: States are owned by the GameStateMachine (pass `new` states, don't delete)
- **Deferred Deletion**: Prevents use-after-free bugs during state transitions
- **Transition Hooks**: `onEnter()`, `onExit()` called on state changes
- **Stack Management**: Supports nested states with proper cleanup
- **Frame Pacing** (v1.3+): protected non-virtual `double CapFrameRate(double maxDeltaSeconds = 0.05)` sleeps out the 60 FPS budget, returns the frame time in seconds and rolls `millisecondsPreviousFrame` forward. Pass `0` to leave the delta unclamped

#### Input Handling
- **Keyboard/Mouse**: SDL2 event loop abstraction
- **Gamepad** (v1.3+):
  - `Gamepad`: SDL_GameController wrapper - `OpenFirstAttached()`, `HandleEvent()`, `Update()`, `Down`/`Pressed`/`Released`, `Current()`, `Connected()`, `Name()`, `SetDeadzone()`, `Shutdown()`
  - `GamepadButton`, `GamepadState`, and the free `GamepadDown`/`GamepadPressed`/`GamepadReleased`/`GamepadNormaliseStick` helpers
- **Touch Controls** (v1.2+): 
  - `TouchZone`: Rectangular hit-test zones
  - `VirtualGamepad`: Circular D-pad + SNES-style action diamond
  - Pure C++ implementation, no external dependencies

---

## Version History (from CHANGELOG.md)

> **Current release: v1.3.0.** The entries below cover v1.3.0 and then stop at
> v1.2.0; the older ones are kept only as a summary of the early line.
> `CHANGELOG.md` is authoritative and is the only place v1.2.1 through v1.2.6
> are written up - the safe accessors (`TryGetComponent`, `IsAlive`,
> `DoesTagExist`), the `kNetControlClose` wire-format change, the
> `TileMapLoader` failure reporting, and the v1.2.6 build fixes.

### v1.3.0 - 2026-08-23
**Major Update**: Contact reporting, cached fonts and sounds, an engine gamepad, and an installable starter game

> **Upgrading from 1.2.x needs a REBUILD, not just a relink.** `AssetStore`
> gained font and sound caches, so `sizeof(AssetStore)` went 112 → 208 bytes.
> Games allocate the store themselves, so a binary built against 1.2.x headers
> allocates 112 bytes and hands them to a 1.3.0 constructor. This is a
> deliberate, one-off exception to the 1.x layout promise.

#### Added
- `common/collision/shapes.h` - the collision math, with no ECS behind it: `ContactAABB`, `ContactCircle`, and `Overlaps` / `Manifold` / `ClosestPointOn` / `MinimumTranslation` as free functions. Includes glm and nothing else from the engine, so a game using none of the entity machinery can still use it. One solver per shape pair, so `Overlaps` and `Manifold` cannot disagree
- `common/systems/contact.h` - `ContactSystem`: reports AABB overlaps as `Contact{a, b, normal, depth}`, sorted by `(a.id, b.id)` with `a` always the lower id. `SetOnBeginContact`/`SetOnEndContact` fire once per pair on transitions; `SetPairFilter` skips pairs, which is where layers, masks and sensors live. It never kills, moves or writes anything
- `common/text.h` - `Text::Draw` / `DrawCentred` / `Measure`, header-only, null-safe, no engine types
- `common/input/gamepad.h` - `Gamepad`, `GamepadButton`, `GamepadState` and the free button/stick helpers
- `AssetStore::AddFont`/`GetFont` and `AddSound`/`GetSound`; `ClearAssets()` now frees all three kinds
- `GameState::CapFrameRate(double maxDeltaSeconds = 0.05)` - the frame-pacing block seven states had written out by hand
- `stormengine2.pc.in` - `make install` generates `$(PREFIX)/lib/pkgconfig/stormengine2.pc` and installs a starter game to `$(PREFIX)/share/stormengine2/template`
- Suite expanded: 319 → 369 specs (contacts, contact events, pair filtering, gamepad, text, frame pacing, font and sound caching)

#### Changed
- `shooter` moved onto `ContactSystem` with a pair filter; `shooter` and `strategy` deleted their local `gamepad.h` copies
- `sports` rebuilt the rink boards as six collider entities and bounces with `glm::reflect` about the contact normal; `RL`/`RT`/`RR`/`RB` are gone
- `puzzle`, `jrpg`, `netplay-checkers` and `sports` draw through the AssetStore font cache and `Text`; `netplay-checkers` also uses `AddSound`
- `platformer`, `nx-platformer`, `android-platformer`, `shooter`, `puzzle` and `jrpg` call `CapFrameRate()`; five of them deleted a shadowed `millisecondsPreviousFrame_`
- Every example is now clang-format clean

### v1.2.0 — 2026-07-10
**Major Update**: Virtual gamepad promoted from Android example to engine core

#### Added
- `common/input/touchControls.h` - Pure touch primitives: `TouchZone`, `TouchPoint`, three-zone scheme
- `common/input/virtualGamepad.h` - Standard mobile layout with SDL-free implementation
- Suite expanded: 130 → 137 specs (d-pad sectors, deadzone, action diamond)

#### Changed
- Examples now use engine touch controls instead of local copies
- Input specs moved from `specs/examples/` to `specs/input/`

### v1.1.1 — 2026-07-09
**Bug Fix**: `.gitignore` fixed - Android example's `main` directory was incorrectly excluded

### v1.1.0 — 2026-07-09
**First Minor Release**: Android platform target with full SDL surface support

#### Added
- `examples/android-platformer/` - APK build via Gradle + CMake + NDK
- Pinned submodules: SDL2 2.30.11, SDL_image 2.8.8 (no libpng), SDL_ttf 2.22.0 (FreeType vendored), SDL_mixer 2.8.1 (wav/mp3/ogg built-in)
- Suite expanded: 125 → 130 specs

### v1.0.2 — 2026-07-08
**Critical Fix**: GameStateMachine deferred deletion to prevent use-after-free during state transitions

#### Fixed
- `gameStateMachine.cpp` - Deletion now happens at next tick, not inline
- Suite expanded: 113 → 125 tests (all passing)

### v1.0.1 — 2026-07-08
**Post-v1 Code Review**: Memory/correctness fixes and ECS hardening

#### Fixed
- `gameStateMachine.cpp` - Machine now owns all states, prevents leaks
- `ecs.cpp` - Killed entities release tags, tag reuse moves correctly
- `assetStore.cpp` - Proper texture loading error handling
- `logger.cpp` - Static log history capped at 1000 entries
- `ecs.h` - RemoveSystem no-ops when absent, GetSystem throws for missing systems

#### Changed
- Entity shrunk from ~90 to 16 bytes (removed unused Logger members)
- Component pools grow geometrically instead of O(n²) copying
- GameStateMachine ownership contract enforced

### v1.0.0 — 2026-06-21
**First Stable Release**: Engine API locked for 1.x line

#### Added
- `specs/xmlLoader.spec.cpp` - Full XmlLoader coverage (textures, objects, defaults)
- `specs/registry.spec.cpp` - Comprehensive Registry tests
- `specs/tilemapLoaderEditor.spec.cpp` - Editor map format coverage
- Suite expanded: 57 → 113 tests

### v0.5.0 — 2026-06-01
**Major Feature**: ECS implementation and tutorial documentation

#### Added
- Full ECS: Registry, Entity, System, Component, Pool classes
- Built-in components: Transform, RigidBody, Sprite, Animation, BoxCollider
- Built-in systems: Movement, Render, Animation, Collision, RenderCollider
- `XmlLoader` for XML-based asset configuration
- Complete TUTORIAL.md with working examples

### v0.4.0 — 2023-07-30
**Editor Update**: Map editor file dialog implemented

### v0.3.0 — 2023-07-17
**Strategy Game Example**: Added strategy game with PlayState integration

---

## Key Files Reference

### Core Engine (common/)
| File | Purpose |
|------|---------|
| `ecs.h/cpp` | Entity, Component, System, Pool and Registry classes |
| `gameStateMachine.h/cpp` | State management with deferred deletion |
| `states/gameStateBase.h` | The `GameState` interface alone - 80,265 preprocessed lines. Include this when the game does not use the ECS |
| `states/gameState.h` | `gameStateBase.h` plus the convenience includes (every component, every system, AssetStore, Logger, TileMapLoader) - 146,748 lines |
| `assetStore.h/cpp` | Texture, font and sound caching |
| `text.h` | One-line SDL_ttf drawing: `Draw`, `DrawCentred`, `Measure` |
| `logger.h/cpp` | Console logging with color codes |
| `tilemapLoader.h/cpp` | Map parsing; editor and legacy CSV formats, auto-detected |
| `xmlLoader.h/cpp` | XML configuration parser |
| `net/net.h` | UDP client-server networking, ported from Teeworlds 0.7.5 |

### Systems (systems/)
| File | Purpose |
|------|---------|
| `contact.h` | `ContactSystem`: contact reporting for box and circle colliders, including box/circle pairs, with begin/end callbacks and a pair filter |
| `movement.h` | `MovementSystem`: integrates `RigidBodyComponent` velocity by delta time |
| `render.h` | `RenderSystem`: z-sorted sprite drawing with optional camera offset |
| `animation.h` | `AnimationSystem`: advances the sprite source rect, looped or one-shot |
| `renderCollider.h` | `RenderColliderSystem`: debug collider outlines, box and circle, resolved through `ContactSystem`'s statics so the overlay cannot disagree with the sweep. Takes an optional camera |

### Input (input/)
| File | Purpose |
|------|---------|
| `keyboard.h` | `Keyboard`: edge-triggered key state, header-only, does not poll |
| `gamepad.h` | `Gamepad`: SDL_GameController wrapper, button edges, stick deadzone |
| `touchControls.h` | Touch primitives: TouchZone, TouchPoint |
| `virtualGamepad.h` | Mobile D-pad + action diamond layout |

### Examples
- **platformer**: Scrolling 2-D platformer - tilemap, gravity, AABB resolution, camera
- **android-platformer**: APK build driven by the engine's virtual gamepad
- **nx-platformer**: Nintendo Switch homebrew example
- **shooter**: *1945*, a vertical shoot-'em-up - three-state stack, HUD, gamepad, `ContactSystem`
- **puzzle**: *Storm Tetris*, Tetris-style ECS demonstration
- **jrpg**: Top-down RPG - tilemap world, NPC interaction, typewriter dialogue
- **sports**: *Storm Hockey*, top-down hockey - custom physics systems and gamepad support
- **strategy**: *Realms*, a *Dragon Force*-style campaign map with pushed side-on battles
- **netchat / netrepl / netplay-checkers**: networking demos - console chat, 60 Hz snapshot deltas, and graphical authoritative-netplay checkers

---

## Build System

### Desktop (Debian/Ubuntu)

There is no CMake build and no plain `Makefile` at the repo root - every root
invocation needs `-f Makefile.debian`.

```bash
# Prerequisites: SDL2, SDL_image, SDL_ttf, SDL_mixer, glm, GTK3,
# plus Igloo + snowhouse built from source for the specs (see README.md).
# tinyxml2 is NOT one: from 2.1.1 it is compiled in from vendor/android/tinyxml2
# and libtinyxml2-dev is no longer needed to build or to consume the engine.
# Also fetch that submodule:  git submodule update --init vendor/android/tinyxml2
make -f Makefile.debian            # runs the spec suite, then builds ./bin/libstormenginev2.so
make -f Makefile.debian target     # library only
sudo make -f Makefile.debian install
```

`install` writes the headers, the `.so`, `lib/pkgconfig/stormengine2.pc` and a
starter game under `share/stormengine2/template`. Both `PREFIX` (default
`/usr/local`) and `DESTDIR` are honoured; the release workflow packages the
`.deb` by staging through `DESTDIR`.

Build a game against the installed engine through pkg-config. `-lstormenginev2`
on its own fails the moment the game calls SDL directly:

```bash
g++ -std=c++17 mygame.cpp $(pkg-config --cflags --libs stormengine2) -o mygame
```

Or start from the installed template:

```bash
cp -r /usr/local/share/stormengine2/template ~/mygame
cd ~/mygame && make run
```

### Windows (MinGW-w64 cross-compile from Linux)
```bash
sudo apt install mingw-w64 cmake
make -f Makefile.win        # build/win/libstormenginev2.dll + build/win/tests.exe
make -f Makefile.win dist   # stormengine2-<version>-win64.zip — the release artifact
```

The zip is an SDK, not just the DLL: it carries the import library and the
`stormengine2`, `SDL2` and `glm` headers, because 12 engine headers include
`<SDL2/SDL.h>` and 6 include `<glm/...>` and Windows has no package manager to
fetch them from. **MinGW-w64 only — MSVC cannot link it**, the import library
and the C++ ABI are GCC's.

Do not add `-static-libgcc` / `-static-libstdc++` to the DLL or to anything that
links it: statically linking the runtime into a *shared* library absorbs
libgcc's unwinder, and MinGW exports every symbol by default, so the DLL
re-exports `_Unwind_Resume` and consumers fail on a duplicate definition. That
shipped in 2.1.0 and was fixed in 2.1.1.

### Android
```bash
# Requires: Android cmdline-tools + NDK locally
cd examples/android-platformer/
./gradlew assembleDebug
```

### Nintendo Switch (devkitPro only, not released)
```bash
export DEVKITPRO=/opt/devkitpro     # the makefile hard-errors without it
cd examples/nx-platformer && make
```
- Assets embedded via romfs
- Controller input via libnx PadState

---

## Testing Strategy

The project uses a comprehensive test suite organized by feature:

| Test Suite | Specs | Purpose |
|------------|-------|---------|
| `registry.spec.cpp` | 58 | Component/system registration, entity management |
| `ecs.spec.cpp` | 57 | Entity handles and generations, component misses, the 64-component ceiling, the disabled latch |
| `net/*.spec.cpp` | 122 | Packets, varints, snapshots, connections, loopback |
| `collision/shapes.spec.cpp` | 27 | Box and circle overlap, manifolds, the Overlaps/Manifold agreement contract |
| `input/actionMap.spec.cpp` | 27 | One action across keyboard, gamepad, virtual pad and touch; edge semantics |
| `states/gameStateMachine.spec.cpp` | 17 | State transitions, deferred deletion, ownership |
| `xmlLoader.spec.cpp` | 15 | XML texture and object definitions |
| `tilemapLoaderEditor.spec.cpp` | 12 | Editor map format, including the animation and collider-offset fields |
| `systems/render.spec.cpp` | 12 | Source-rect bounds, z-ordering, camera offset |
| `systems/contact.spec.cpp` + `contactEvents` + `contactFiltering` + `contactCircle` | 40 | Manifolds and ordering, begin/end events, pair filtering, and mixed box/circle sweeps |
| `input/gamepad.spec.cpp` | 11 | Button edges, deadzone and stick normalisation |
| `systemMembership.spec.cpp` | 10 | Membership at admission, and the retrofit for a late system |
| `input/virtualGamepad.spec.cpp` + `touchControls` | 15 | Touch zone hit-testing, d-pad sectors, action diamond |
| `components/*.spec.cpp` | 25 | Component construction and defaults |
| `assetStore.spec.cpp` | 8 | Texture, font and sound caching, and clearing |
| `compat/global.spec.cpp` | 8 | The 1.x compatibility bridge, checked against a generated probe |
| `logger.spec.cpp` | 7 | Log routing and per-instance callbacks |
| `text.spec.cpp` | 6 | Null-safe measure and draw |
| `tilemapLoader.spec.cpp` | 6 | CSV map parsing |
| `systems/renderCollider.spec.cpp` | 18 | Debug collider rendering, box and circle, camera panning and the drawable bounds, against a real software renderer |
| `systems/movement.spec.cpp` | 7 | Velocity integration |
| `systems/animation.spec.cpp` | 5 | Frame advance and looping |
| `states/gameState.spec.cpp` + `gameStateBase` | 8 | `CapFrameRate` pacing, delta clamping, the slim interface |
| `layout.spec.cpp` | 1 | The ABI sizes, and the value of `MAX_COMPONENTS` |
| `gameStateMachineSlim.spec.cpp` + `gameStateMachineNonCopyable` | 3 | The slim header stays slim; the machine is not copyable |

**Total**: 531 specs on `main` after the circle-collider branch, plus whatever
the working branch adds; all passing. Counts are `It(` tallies per file, and they rot — run
`make -f Makefile.debian test` for the authoritative figure. (This table
previously listed `systems/collision.spec.cpp`, which went away with
`CollisionSystem` in 2.0.0, and totalled 369 "as of v1.3.0".)

The suite covers `common/` only - `TESTSRCS` is `find specs` plus `find
common`, so nothing under `editor/` or `examples/` is compiled into
`./bin/tests`. Run `./bin/tests` from the repo root; several specs hardcode
`./specs/assets/...` paths.

---

## API Usage Examples

### Creating an Entity with Components
```cpp
auto entity = registry.CreateEntity();
entity.AddComponent<TransformComponent>(glm::vec2(0, 0), glm::vec2(1, 1), 0.0);
entity.AddComponent<SpriteComponent>("player-sprite", 48, 48, 1);  // id, w, h, zIndex
entity.AddComponent<BoxColliderComponent>(48, 48);                 // w, h
```

A round body takes `CircleColliderComponent` instead - one collider or the
other, never both:

```cpp
entity.AddComponent<CircleColliderComponent>(24.0f, glm::vec2(24, 24));
//                                           radius, offset to the CENTRE
```

The offset places the centre, not a corner, so centring a circle on a 48x48
sprite drawn from `transform.position` wants `{24, 24}`. `ContactSystem` sweeps
both shapes together and pairs them against each other.

Add every component the entity will ever need before the `registry.Update()`
that admits it: system membership is computed once, and a component added later
will not move the entity into a matching system. `ContactSystem` and
`RenderColliderSystem` are the exceptions — both require `TransformComponent`
alone and re-read the collider each frame, so a box or circle collider can be
added or removed after admission.

### Adding a System
```cpp
class HealthSystem : public System {
public:
  HealthSystem() { RequireComponent<HealthComponent>(); }

  void Update() {
    for (auto &entity : GetSystemEntities()) {
      auto &health = entity.GetComponent<HealthComponent>();
      // Process entity...
    }
  }
};

registry.AddSystem<HealthSystem>();          // register BEFORE creating entities
registry.GetSystem<HealthSystem>().Update(); // no scheduler; call it yourself
```

### Reacting to Contacts
```cpp
registry.AddSystem<ContactSystem>();
auto &contacts = registry.GetSystem<ContactSystem>();

// Layers, masks and sensors live in the filter - skipped pairs never build a
// manifold and never fire an event.
contacts.SetPairFilter([](const Entity &a, const Entity &b) {
  return a.HasComponent<BulletComponent>() || b.HasComponent<BulletComponent>();
});

contacts.Update();  // reports; never kills or moves anything
for (const auto &c : contacts.GetContacts()) {
  // c.a always holds the lower id; c.normal points from a toward b along the
  // axis of least penetration, and c.depth is the overlap on that axis.
}
```

### Pacing a Frame and Drawing Text
```cpp
void PlayState::update() {
  const double dt = CapFrameRate();  // sleeps out the 60 FPS budget, clamps a hitch
  registry_.Update();                // flush pending adds and kills FIRST
  registry_.GetSystem<MovementSystem>().Update(dt);
}

// onEnter(): one font id per point size.
assetStore_->AddFont("hud-24", "./assets/font.ttf", 24);

// render():
Text::DrawCentred(renderer_, assetStore_->GetFont("hud-24"), "Score: 100",
                  windowWidth_ / 2, 24, SDL_Color{255, 255, 255, 255});
```

### State Machine Usage
```cpp
auto state = new PlayState();
gameMachine.changeState(state);  // State is owned by machine, don't delete!
// Later: gameMachine.popState();  // Automatically cleans up
```

---

## Documentation Resources

1. **README.md** - Project overview, build instructions, example list
2. **TUTORIAL.md** - Complete ECS tutorial with working code examples
3. **CHANGELOG.md** - Version history, feature tracking and behavioural contracts
4. **KNOWN_ISSUES.md** - Real defects deliberately left unfixed in the 1.x line
5. **CODING.md** - Duplication and cohesion tenets the engine is written to
6. **docs/networking.md** - The `net/` module in depth
7. **docs/TECH_DEBT.md** - Ranked technical-debt ledger
8. **editor/README.md** + **editor/TUTORIAL.md** - The tilemap and collider painter
9. **template/README.md** - The starter game `make install` ships
10. **Specs Directory** - Unit test specifications for all core features

---

## License & Credits

- **License**: WTFPL (see `LICENSE.md`). `common/net/` is a port of Teeworlds 0.7.5 networking, zlib-licensed
- **Dependencies**: SDL2, SDL_image, SDL_ttf, SDL_mixer, glm (apt on desktop; pinned submodules for the Android build). tinyxml2 is vendored (`vendor/android/tinyxml2`) and compiled into the library on every platform since 2.1.1 — it was the one distro-specific soname in the `.deb` and made the package uninstallable off the build container's distro
- **Contributors**: See GitHub contributors and commit history

---

*Last updated: 2026-08-23 (v1.3.0)*
