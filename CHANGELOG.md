# Changelog

All notable changes to storm-engine-v2 are documented here.

---

## [Unreleased] — 2026-05-30

### Added
- `examples/puzzle/` — Tetris-style puzzle game demonstrating ECS usage: board cells and active piece cells are entities, `TetrisCellComponent` stores board coordinates, `TetrisSyncSystem` maps board positions to screen positions, and the engine's `RenderSystem` handles all drawing
- `TUTORIAL.md` — full written tutorial covering ECS concepts, project setup, all built-in components and systems, writing custom components and systems, AssetStore, Logger, tags, groups, and a complete working PlayState example

### Changed
- `examples/shooter/` — rebuilt as a proper Alien Attack-style scrolling shooter using the engine's ECS; helicopter player with animation, three enemy types, bullet spawning with cooldown, enemy spawn intervals, tiled scrolling cloud background, and DAS (delayed auto-shift) movement
- `examples/shooter/` — switched player sprite to `helicopter.png` and enemy to `helicopter2.png` for correct facing direction; fixed `RenderSystem` to respect `SpriteComponent::flip` (was hardcoded to `SDL_FLIP_NONE`)
- `base.mk` — removed stray `#!/bin/sh` shebang, fixed `-isystem` flag to `-I/usr/local/include`, changed old-style `.cpp.o` suffix rule to modern `%.o: %.cpp` pattern rule
- `common/systems/collision.h` — fixed copy-paste bug where `entBXmax` and `entBYmax` were both computed using `colliderComponentA` instead of `colliderComponentB`
- `common/gameStateMachine.cpp` — fixed crash in `popState()` when calling `resume()` on an empty state stack
- `common/tilemapLoader.cpp` — added early return after null `IMG_Load` check to prevent null dereference
- `examples/strategy/src/game.cpp` — fixed missing `std::move(assetStore)` argument when constructing `PlayState`
- `examples/strategy/src/states/playState.cpp` — fixed `RenderSystem::Update` call to correctly dereference `AssetStore_Ptr` (`*assetStore_`), fixed `windowWidth` typo

---

## [0.4.0] — 2023-07-30

### Added
- Map editor: file dialog implemented with NFD (Native File Dialog)

### Changed
- Editor: GTK integration, editor compilation fixes
- Registry: continued paring and refactoring

---

## [0.3.0] — 2023-07-17

### Added
- `examples/strategy/` — strategy game example with `PlayState`, `GameStateMachine` integration, tilemap rendering, tank and truck entities with collision
- `processInput` added to all game states via the `GameState` base interface

### Changed
- Game state machine test completed
- Memory leak fixes

---

## [0.2.0] — 2023-06-04

### Added
- Full ECS implementation: `Registry`, `Entity`, `System`, `Component`, `Pool`
- Built-in components: `TransformComponent`, `RigidBodyComponent`, `SpriteComponent`, `AnimationComponent`, `BoxColliderComponent`
- Built-in systems: `MovementSystem`, `RenderSystem`, `AnimationSystem`, `CollisionSystem`, `RenderColliderSystem`
- `AssetStore` for texture management
- `Logger` with color-coded output and callback support
- Entity tagging and grouping (`Tag`, `Group`, `GetEntityByTag`, `GetEntitiesByGroup`)
- Component tests and Registry unit tests

### Changed
- Build system refactored: Makefiles restructured, dynamic shared library build for engine reuse
- Multi-platform build support added

---

## [0.1.0] — 2023-01-30

### Added
- Initial project setup with LICENSE
- Basic Logger implementation (`Log`, `Err`, timestamped output)
- Spec/test conventions established
- ECS foundation: Pool class, systems logic skeleton
