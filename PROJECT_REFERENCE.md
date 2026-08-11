# Storm Engine v2 - Project Reference Document

## Overview

**Storm Engine v2** is a modern, ECS-based game engine written in C++ with a focus on simplicity, correctness, and cross-platform support. It provides a clean abstraction over SDL2 for rendering, input handling, audio, and asset management, while maintaining a robust Entity-Component-System architecture for game logic.

### Key Features
- **ECS Architecture**: Clean separation of entities, components, systems, and pools with automatic memory management
- **Cross-Platform**: Desktop (Windows/Linux/macOS), Android, Nintendo Switch homebrew support
- **Asset Management**: Built-in texture loading from SDL_image, XML-based asset configuration
- **Game State Machine**: Robust state management with deferred deletion to prevent use-after-free bugs
- **Input Handling**: Keyboard/mouse, touch controls for mobile, and platform-specific input abstractions
- **Rendering**: Full SDL2 integration with custom RenderSystem for efficient sprite rendering
- **Collision Detection**: AABB-based collision system with configurable collider components

---

## Architecture Summary

### Core Components (from TUTORIAL.md)

#### ECS Foundation
```cpp
// Entity: unique identifier, owns all its components and systems
Entity entity;

// Component: data attached to an entity
TransformComponent transform;
SpriteComponent sprite;
BoxColliderComponent collider;

// System: logic that operates on entities with matching signatures
void MoveSystem(Entity& e) { ... }
```

#### Asset Management (AssetStore)
- **Texture Loading**: `IMG_Load` + `SDL_CreateTextureFromSurface` with error handling
- **XML Parsing**: `XmlLoader` for asset configuration files
- **Memory Management**: Automatic cleanup on entity destruction or state transition

#### Game State Machine
- **State Ownership**: States are owned by the GameStateMachine (pass `new` states, don't delete)
- **Deferred Deletion**: Prevents use-after-free bugs during state transitions
- **Transition Hooks**: `onEnter()`, `onExit()` called on state changes
- **Stack Management**: Supports nested states with proper cleanup

#### Input Handling
- **Keyboard/Mouse**: SDL2 event loop abstraction
- **Touch Controls** (v1.2+): 
  - `TouchZone`: Rectangular hit-test zones
  - `VirtualGamepad`: Circular D-pad + SNES-style action diamond
  - Pure C++ implementation, no external dependencies

---

## Version History (from CHANGELOG.md)

> **Current release: v1.2.6.** The entries below stop at v1.2.0 and are kept
> only as a summary of the early line. `CHANGELOG.md` is authoritative and
> covers v1.2.1 through v1.2.6 — the safe accessors (`TryGetComponent`,
> `IsAlive`, `DoesTagExist`), the `kNetControlClose` wire-format change, the
> `TileMapLoader` failure reporting, and the v1.2.6 build fixes.

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
| `ecs.h/cpp` | Entity, Component, System, Pool classes |
| `gameStateMachine.h/cpp` | State management with deferred deletion |
| `assetStore.h/cpp` | Texture loading and asset management |
| `logger.h/cpp` | Console logging with color codes |
| `registry.h/cpp` | ECS registration system |
| `tilemapLoader.h/cpp` | Editor map format parsing |
| `xmlLoader.h/cpp` | XML configuration parser |

### Input (input/)
| File | Purpose |
|------|---------|
| `touchControls.h` | Touch primitives: TouchZone, TouchPoint |
| `virtualGamepad.h` | Mobile D-pad + action diamond layout |

### Examples
- **android-platformer**: APK build with touch controls
- **nx-platformer**: Nintendo Switch homebrew example
- **shooter**: Alien Attack-style scrolling shooter
- **puzzle**: Tetris-style ECS demonstration
- **jrpg**: RPG state machine examples
- **strategy**: Tank/truck collision game

---

## Build System

### Desktop (Windows/Linux/macOS)
```bash
# Prerequisites: SDL2, SDL_image, tinyxml2, glm
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Android
```bash
# Requires: Android cmdline-tools + NDK locally
cd examples/android-platformer/
./gradlew assembleDebug
```

### Nintendo Switch (devkitPro only, not released)
- Build with `build-nx` script in devkitPro environment
- Assets embedded via romfs
- Controller input via libnx PadState

---

## Testing Strategy

The project uses a comprehensive test suite organized by feature:

| Test Suite | Specs | Purpose |
|------------|-------|---------|
| `registry.spec.cpp` | 20+ | Component/system registration, entity management |
| `gameStateMachine.spec.cpp` | 35+ | State transitions, deferred deletion, ownership |
| `tilemapLoaderEditor.spec.cpp` | 15+ | Map format parsing and rendering |
| `components/animation.spec.cpp` | 8+ | Animation frame handling, loop control |
| `components/sprite.spec.cpp` | 7+ | Sprite flipping, offset, fixed positioning |
| `input/touchControls.spec.cpp` | 10+ | Touch zone hit-testing, multi-finger evaluation |

**Total**: 319 specs as of v1.2.6, all passing. The per-file counts above were
taken at v1.2.0 and have not been retallied; run `make -f Makefile.debian test`
for the current figure.

---

## API Usage Examples

### Creating an Entity with Components
```cpp
auto entity = registry.CreateEntity();
entity.AddComponent<TransformComponent>(vec2{0, 0});
entity.AddComponent<SpriteComponent>("player.png");
entity.AddComponent<BoxColliderComponent>({48, 48});
```

### Adding a System
```cpp
void MySystem(Entity& e) {
    auto& transform = e.GetComponent<TransformComponent>();
    // Process entity...
}
registry.AddSystem<MySystem>({"Transform", "Sprite"});
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
3. **CHANGELOG.md** - Version history and feature tracking
4. **Specs Directory** - Unit test specifications for all core features

---

## License & Credits

- **License**: MIT (check LICENSE file)
- **Dependencies**: SDL2, SDL_image, tinyxml2, glm (all vendored or pinned versions)
- **Contributors**: See GitHub contributors and commit history

---

*Last updated: 2026-07-10 (v1.2.0)*
