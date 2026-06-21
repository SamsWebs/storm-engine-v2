# Changelog

## [1.0.0] — 2026-06-21

First stable release. The engine API (`Registry`, `GameStateMachine`, `XmlLoader`, `TileMapLoader`, components, and systems) is now considered locked for the 1.x line.

### Added
- `specs/xmlLoader.spec.cpp` + `specs/assets/xml/states.xml` — full coverage of the `XmlLoader` parser: `IsValid`, `GetTextures`, `GetObjects`, including default values, fractional float attributes, the unknown-attribute map, and missing states/sections
- `specs/registry.spec.cpp` — comprehensive `Registry` coverage: component add/remove/has/get, system management, signature-based entity/system matching, tag management, group management, entity kill with id recycling, and the deferred `Update()` add/kill queue
- `specs/tilemapLoaderEditor.spec.cpp` + `specs/assets/tilemaps/editor.map` — coverage of the editor map format: world-to-grid position math, `tileSize` variants, per-tile scale, and collider flags
- `specs/states/gameStateMachine.spec.cpp` — expanded coverage for `changeState` (including the same-state-id no-op), `resume()` on pop, `clean()`, and empty-stack safety
- `specs/components/animation.spec.cpp`, `specs/components/sprite.spec.cpp` — deeper component coverage: animation `vertical`/`isLooped` constructor argument order, frame offset and start frame; sprite `flip`, `isFixed`, and `offset`

### Notes
- Engine unit test suite expanded from 57 to 113 tests, all passing
- No engine source changes were required for testability — the existing API was already test-friendly via fixtures and direct construction

## [0.5.0] — 2026-06-01

### Added
- `examples/puzzle/` — Tetris-style puzzle game demonstrating ECS usage: board cells and active piece cells are entities, `TetrisCellComponent` stores board coordinates, `TetrisSyncSystem` maps board positions to screen positions, and the engine's `RenderSystem` handles all drawing
- `TUTORIAL.md` — full written tutorial covering ECS concepts, project setup, all built-in components and systems, writing custom components and systems, AssetStore, Logger, tags, groups, and a complete working PlayState example
- `examples/nx-platformer/` — Nintendo Switch homebrew example built with devkitPro; assets embedded via romfs, controller input via libnx `PadState`, fullscreen SDL window; use as a starting point for Switch projects
- `common/xmlLoader.h` + `common/xmlLoader.cpp` — `XmlLoader` promoted to a first-class engine type; parses `<States>/<STATE>/<TEXTURES>` and `<OBJECTS>` from XML, returns plain `XmlTextureDef` / `XmlObjectDef` structs with no ECS or AssetStore coupling
- `common/xmlLoader.h` — `LoadTexturesFromXml` free function: reads texture definitions for a state and loads them directly into the `AssetStore`

### Changed
- `examples/shooter/` — rebuilt as a proper Alien Attack-style scrolling shooter using the engine's ECS; helicopter player with animation, three enemy types, bullet spawning with cooldown, enemy spawn intervals, tiled scrolling cloud background, and DAS (delayed auto-shift) movement
- `examples/shooter/` — switched player sprite to `helicopter.png` and enemy to `helicopter2.png` for correct facing direction; fixed `RenderSystem` to respect `SpriteComponent::flip` (was hardcoded to `SDL_FLIP_NONE`)
- `examples/shooter/` — removed shooter-specific `XmlLoader.h`; spawn logic inlined into `playState.cpp`, texture loading now uses the engine's `LoadTexturesFromXml`
- `base.mk` — removed stray `#!/bin/sh` shebang, fixed `-isystem` flag to `-I/usr/local/include`, changed old-style `.cpp.o` suffix rule to modern `%.o: %.cpp` pattern rule
- `common/systems/collision.h` — fixed copy-paste bug where `entBXmax` and `entBYmax` were both computed using `colliderComponentA` instead of `colliderComponentB`
- `common/gameStateMachine.cpp` — fixed crash in `popState()` when calling `resume()` on an empty state stack
- `common/tilemapLoader.cpp` — added early return after null `IMG_Load` check to prevent null dereference
- `examples/strategy/src/game.cpp` — fixed missing `std::move(assetStore)` argument when constructing `PlayState`
- `examples/strategy/src/states/playState.cpp` — fixed `RenderSystem::Update` call to correctly dereference `AssetStore_Ptr` (`*assetStore_`), fixed `windowWidth` typo
- `README.md` — expanded examples section with per-example resource loading explanations (tile editor `.map`, hard-coded ECS, XML via `XmlLoader`)

### Removed
- `Dockerfile.nx` and the `build-nx` CI job — Switch builds are not released as artifacts; developers targeting Switch should clone the source and build with devkitPro directly

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
