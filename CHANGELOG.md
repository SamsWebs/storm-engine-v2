# Changelog

## [Unreleased]

Memory-safety and correctness pass over the networking layer and the ECS, plus a Windows cross-build. No breaking changes — every addition is additive.

### Added

- `Registry::TryGetComponent<T>()` / `Entity::TryGetComponent<T>()` — return `nullptr` when the component is absent. This is the correct accessor whenever a miss is possible; `GetComponent` must return a reference and therefore cannot report one.
- `Registry::IsAlive(Entity)` and `Registry::DoesTagExist(const std::string &)` — guards for the accessors that cannot fail safely on their own.
- Windows cross-build via MinGW-w64: `Makefile.win`, `cmake/toolchain-mingw64.cmake`, `examples/examples.win.mk`. Builds `libstormenginev2.dll` and the spec suite. Not covered by CI, which builds `Dockerfile.debian` only.
- `KNOWN_ISSUES.md` — defects that cannot be fixed within the frozen 1.x API, each with a workaround and the reason. Candidates for a future v3.
- README section documenting the 32 component-type limit: it is per binary rather than per `Registry`, and it counts types rather than instances.

### Fixed

- **`NetControlPacket::Unpack` overran its payload buffer.** `payloadSize` was assigned before an unbounded `memcpy`, so a full-MTU control datagram overwrote it with attacker-chosen bytes — up to a 64 KB out-of-bounds read on the client, which then copied it into a `std::string` and logged it. Unauthenticated: one UDP datagram.
- **`NetServer::SendControl` smashed the stack** on a `DisconnectClient` reason longer than the payload buffer.
- **`BufferVital`'s ring wrap** left a tail gap the consumers did not know about, so retransmits carried the wrong bytes after roughly 16 KB of vital traffic.
- **`NetMessageReader::ReadString` leaked the caller's buffer.** An unterminated wire string left stale bytes in `out` and reported success, so a truncated or hostile packet made the caller read what the peer never sent. It now fails closed and null-terminates on every exit path.
- **Handshake nonces were predictable.** `NetRandom32` was a raw xorshift64, and six observed nonces recovered its state — voiding the cookie handshake's anti-spoofing guarantee. A slot's server nonce is also rotated on any CONNECT other than a genuine mid-handshake retry: the nonce is the connection token, travels in cleartext in every packet header, and equality against it is the only authentication on an inbound connected packet.
- **`Registry::GetComponent` returned a shared mutable static on a miss**, so a write through one miss surfaced in every later miss, including across `Registry` instances.
- **`std::bitset` could throw out of `ecs.h`.** Component ids are now range-checked and the accesses use `operator[]`, which removes `__throw_out_of_range_fmt` from the header entirely — it aborted rather than threw under the Switch build's `-fno-exceptions`.
- **`Registry::GetEntitiesByGroup` aborted on an unknown group** (`.at()` on a missing key). Returns an empty vector now.
- **`KillEntity` rejects an already-dead entity in O(1)**, replacing a linear scan on a per-frame path.
- **Networking never compiled on Android.** `app/jni/CMakeLists.txt` globbed `common/*.cpp` non-recursively, silently dropping all seven `common/net/` translation units, and the manifest lacked `android.permission.INTERNET` — which fails at runtime, not at build time. Verified: 490 `Net*` symbols now present in `libmain.so`, both ABIs.
- Non-canonical varints and trailing bytes in `NetSnapshotDelta::Apply` are rejected; snapshot keys with item type ≥ `0x8000` no longer encode as negative varints (receiver-side only, no wire-format change).
- `editor/include/stormengine2/components/sprite.h` was a byte-identical copy shadowing the installed header, where it could only ever hide upstream fixes.

### Changed

- The logger no longer flushes on every line — only on errors. ECS miss diagnostics are throttled, so a game missing every frame no longer does 60 flushed writes a second.
- Removed the dead root `Makefile.nx` (it recursed into a root `Makefile` that does not exist; the working Switch path is `examples/nx-platformer/`), a stray 0-byte `kNetMaxPacketSize` file, and the tracked `editor/imgui.ini` runtime state.
- `.dockerignore` no longer lets host-built object files into the Debian image, where they could be linked stale.

### Notes

- Suite: 210 → 273 specs. The new coverage is adversarial — truncated and oversize packets, malformed deltas, component-id overflow, recycled-id handles.
- Two ECS defects are deliberately left open because they cannot be fixed without breaking the 1.x ABI: a stale `Entity` handle whose id has been recycled kills the new entity, and a system that overflows the component cap matches every entity instead of none. Both are documented in `KNOWN_ISSUES.md`, and two specs pin the current wrong behaviour so a future fix has to update them.

## [1.2.1] — 2026-07-31

UDP networking, ported from Teeworlds 0.7.5 (zlib). Released as an automatic patch bump; this entry is retroactive.

### Added

- `common/net/` and the umbrella header `<stormengine2/net/net.h>` — client/server LAN play over raw non-blocking UDP sockets, with no SDL_net or enet dependency.
  - `NetServer` / `NetClient` — cookie handshake, per-IP connection caps, bans, kick and timeout handling.
  - `NetConnection` — reliability layer: vital chunks with acks and resends, non-vital chunks that may be dropped or reordered. Owns no socket; the caller supplies a send callback.
  - `NetSnapshot` / `NetSnapshotDelta` / `NetSnapshotCache` — tick state replication with per-client deltas and a 16-tick prediction cache.
  - `NetMessageWriter` / `NetMessageReader` — message packing for game-defined message ids.
  - `NetSocket` — the only OS-touching piece (BSD sockets, winsock behind `_WIN32`).
- Examples: `netchat` (console host/join with reliable echo), `netrepl` (60 Hz authoritative host demonstrating snapshot deltas), `netplay-checkers` (graphical, ECS, full-state broadcast).
- `docs/networking.md` — wire format and integration recipes.

### Notes

- Suite: 137 → 210 specs.
- The module is SDL-free and has no coupling to the ECS or the engine tick; games marshal their own components into snapshots.
- Not included in the Switch or Android builds at this release — both globbed engine sources non-recursively. Fixed for Android in the next release; the Switch path remains homebrew-only via devkitPro.

## [1.2.0] — 2026-07-10

Virtual gamepad promoted from the Android platformer into the engine core.

### Added

- `common/input/touchControls.h` — pure touch primitives: `TouchZone` (rect hit-test), `TouchPoint`, and a simple three-zone (◀ ▶ / action) scheme with `MakeDefaultZones` / `EvalTouches`. First-class engine headers under a new `<stormengine2/input/...>` path.
- `common/input/virtualGamepad.h` — the standard mobile layout: a circular d-pad (8-way via angle sectors, with a deadzone) bottom-left and a SNES-style A/B/X/Y action diamond bottom-right. `MakeVPadLayout(w, h)` + `EvalVPad(layout, fingers)`, all SDL-free. Proven on a real device in the Conan the Caveman Android port.

### Changed

- `examples/android-platformer` now includes the touch controls from the engine (`<stormengine2/input/touchControls.h>`) instead of a local copy.
- Specs moved from `specs/examples/` to `specs/input/`; `Dockerfile.debian` no longer needs to copy the example's input dir.

### Notes

- Suite: 130 → 137 specs (the virtual gamepad's d-pad sectors, deadzone, and action diamond).

## [1.1.1] — 2026-07-09

### Fixed

- `.gitignore` — a bare `main` entry matched the Android example's `app/src/main/` directory, silently excluding the `AndroidManifest.xml` and the `SDLActivity` subclass from the 1.1.0 release (the app couldn't be built from a clean clone). Root-anchored it as `/main` and stopped ignoring the committed `gradlew`/`gradlew.bat`.

## [1.1.0] — 2026-07-09

First minor release of the 1.x line: Android as a platform target.

### Added

- `examples/android-platformer/` — first Android target, verified on real hardware over USB debugging: the desktop platformer built as an APK via Gradle + CMake + NDK, with SDL's `SDLActivity` hosting the engine and game in a single JNI library (SDL2/SDL_image shared, tinyxml2 static). On-screen touch pads (◀ ▶ / A) with pure, spec'd zone logic; APK assets extracted to internal storage at first launch so the engine's plain-file I/O works unchanged; fixed logical resolution letterboxed via `SDL_RenderSetLogicalSize`
- Pinned submodules under `vendor/android/`: SDL2 2.30.11, SDL_image 2.8.8 (stb backend, no libpng), SDL_ttf 2.22.0 (vendored FreeType, no HarfBuzz), SDL_mixer 2.8.1 (wav built-in, mp3 via minimp3, ogg via stb_vorbis — no external codec libs), tinyxml2 10.0.0, glm 1.0.1 — the full desktop SDL surface is available on Android

### Notes

- Suite: 125 → 130 specs (touch-zone layout and multi-finger evaluation)
- Requires Android cmdline-tools + NDK locally to build (see the example README); nothing in the engine core changed

## [1.0.2] — 2026-07-08

### Fixed

- `common/gameStateMachine.cpp` — discarded-state deletion is now **deferred to the machine's next tick** instead of happening inline. `changeState`/`popState` are usually called from inside the state being discarded (the normal pattern for in-game transitions), and 1.0.1's inline delete freed the caller's `this` while its member function was still on the stack — a use-after-free for any game that changes state from within a state. Discarded states land in a defunct list swept at the start of the next `processInput`/`update`; `clean()` also sweeps it. The same-state-id duplicate is still freed immediately (it was never entered and has no live call frames)

### Notes

- The state-machine ownership specs now pin the deferred contract, including that the discarded state survives the `changeState` call that removes it

## [1.0.1] — 2026-07-08

Post-v1 code review: engine memory/correctness fixes, ECS edge-case hardening, and editor bug fixes. No new API surface; one behavioral contract is now enforced (see **Changed**).

### Fixed

- `common/gameStateMachine.cpp` — the machine now owns every state it is handed: `popState()` and `changeState()` delete the state they discard, the same-state-id early return frees the rejected duplicate, and `clean()` deletes the whole stack instead of only the top. Previously **every state transition leaked a state** (see 1.0.2 for a follow-up fix to the deletion timing)
- `common/ecs.cpp` — killed entities now release their tag; a recycled entity id no longer inherits the dead entity's tag (`GetEntityByTag` could return the wrong entity)
- `common/assetStore.cpp` — `AddTexture` checks `IMG_Load`/`SDL_CreateTextureFromSurface` failures instead of silently storing a null texture, and re-adding an existing id replaces (and frees) the old texture instead of leaking the new one
- `common/logger.cpp` — the static in-memory log history is capped at 1000 entries; it previously grew unbounded for the whole session
- `common/ecs.cpp` — `TagEntity` is last-write-wins on both sides (retagging replaces, tag reuse moves), and `GroupEntity` moves an entity between groups; the old `emplace` calls silently no-op'd and left the maps inconsistent
- `common/ecs.h` — `RemoveSystem` no-ops when the system is absent and `GetSystem` throws for a missing system; both previously hit end()-iterator undefined behavior
- `examples/jrpg/` — Y-collision tests from the post-X-move position so diagonal movement can't clip corners; removed the ignored `ptSize` parameter, a dead spawn-time texture tint, and the unused `INTERACT_DIST` constant
- `editor/` — un-swapped width/height when placing box colliders (non-square colliders were saved transposed); fixed string-literal pointer arithmetic in two removal logs (undefined behavior); removed a stray trailing `end` that made Save-as-Lua-table exports invalid Lua; fixed the frame limiter (SDL_Delay was unreachable — only vsync capped the editor); shutdown no longer double-frees the window/renderer and now calls `TTF_Quit`; zoom recomputes the camera cull rect instead of compounding it every wheel tick; `AssetManager` no longer inserts null textures on missing-id lookups or failed loads

### Changed

- `AssetStore::GetTexture` returns `nullptr` for a missing id instead of throwing out of `std::map::at` — matching what call sites already assumed
- `GameStateMachine` **owns the states it is handed** (pass `new`-allocated states and do not delete them yourself). State `onExit()` may now run twice on a transition (machine call + destructor) — keep `onExit` idempotent
- `Entity` and `System` no longer carry unused `Logger` members — `Entity` shrinks from ~90 to 16 bytes and is copied everywhere
- Component pools grow geometrically instead of resizing to exactly-n per entity (was O(n²) copying)

### CI

- `build-and-release.yml` (shipped with the v1.0.0 retag) — the workflow now triggers on `v*.*.*` tag pushes and supports `workflow_dispatch`, runs are serialized via a concurrency group, and the `.deb` Homepage points at the correct repository. This is what restored the missing v1.0.0 release assets

### Notes

- Engine unit test suite expanded from 113 to 125 tests, all passing: state-machine ownership, tag-release-on-kill, tag/group replace semantics, and the asset store error contract
- README: documented two upcoming examples (arena survival, menu-flow skeleton)

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
