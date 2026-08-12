# Changelog

## [Unreleased]

### Added

- **`examples/strategy` is a complete game.** The directory previously held a
  modern-military helicopter demo in 485 lines. It is now *Realms*, a
  *Dragon Force*-style strategy game: a top-down campaign map of six castles
  where generals march on a day timer, and a side-on mass battle that resolves
  when two armies meet, with a troop-counter triangle and four battle orders.

  It is the first example to **push** a state rather than replace one. The
  overworld calls `pushState`, so it stays alive underneath the battle with its
  `Registry`, entities and day counter intact, and picks up in `resume()` —
  which fires instead of `onEnter()` on `popState`. The campaign model lives
  above both states, in `Game`, so the battle never holds a pointer into a state
  that may be mid-teardown.

  **Its artwork is not in this repository.** The sprites are Tiny Swords by
  Pixel Frog, free to use but explicitly not redistributable, so the example
  ships code only and the pack is fetched once by the user; running without it
  prints the download URL and exits rather than opening a black window. This is
  the only example that does not run from a fresh clone, and the trade is
  deliberate — the traffic goes to the artist. See
  `examples/strategy/assets/README.md`.

### Fixed

- **A one-shot animation could never be detected as finished.**
  `AnimationSystem` clamps a non-looping animation with
  `currentFrame = min(max(frame, 0), min(last, numFrames - 1))`, so
  `currentFrame` never reaches `numFrames` and the obvious
  `currentFrame >= numFrames` test never fires. Any caller culling one-shot
  effects that way leaks an entity per effect for the lifetime of the state.
  Documented in `examples/strategy/README.md`, and `.claude/SKILL.md` now states
  the correct test.

## [1.2.6] — 2026-08-11

### Fixed

- **`make -j` in an example or the editor could report success and produce no
  binary.** `examples/examples.mk` and `editor/Makefile` declared
  `all: clean $(TARGET)`. Prerequisites are unordered, so a parallel make ran
  `clean` and the build concurrently: `clean`'s repo-wide `.o`/`.d`
  find-delete could land mid-compile, and its `rm -f $(BIN_DIR)/*` could remove
  the executable after the link. make exited 0 in every case, so the failure
  was silent — `make -j12` in `examples/shooter` lost the race once in three
  runs. Both now read `all: $(TARGET)`, matching `Makefile.debian` and
  `examples/examples.win.mk`, which were already written without the clean.
  Header tracking in `base.mk` has made the unconditional clean unnecessary
  since `-MMD -MP` landed.

- **Every example failed to link with `cannot find -lnfd`.** NFD is used by
  exactly one file — the editor's `FileDialogWin.cpp` — and is vendored as a
  header only (`vendor/nfd/nfd.h`), with no library shipped. It sat in
  `base.mk`'s shared `LIB`, which `examples/examples.mk` and `editor/Makefile`
  both inherit. `Makefile.debian` stripped it with a `filter-out`, so the
  engine and the spec suite built fine and the breakage was invisible from the
  repo root; every example was unbuildable on any machine without `libnfd`
  installed. It now lives in an `EDITOR_LIB` variable that only the editor
  links, and the redundant `filter-out` is gone.

- **A from-source build failed to link on Debian and Ubuntu.** `base.mk` passed
  `-llua`, and those distributions ship `liblua5.4.so` with no versionless
  `liblua.so`, so the first link produced `/usr/bin/ld: cannot find -llua` — the
  spec suite, and therefore the default goal, never got past `test-target`.
  Nothing in `common/`, `specs/` or `editor/` includes a Lua header or
  references a `lua_`/`luaL_` symbol: the editor's `.lua` project files are
  parsed by hand and `FileLoader.cpp` only checks the extension, so the
  interpreter was never actually linked for a reason. The flag is gone and
  `liblua5.4-dev` has been dropped from the README prerequisites.

### Added

- **`examples/shooter` is a complete game.** The directory previously held a
  set of unused placeholder assets and a stub. It is now *1945*, a vertically
  scrolling shoot-'em-up with a three-state stack (menu, play, game over),
  wave formations, a barrel roll with invulnerability frames, lives, an
  immediate-mode HUD, and `SDL_GameController` support merged with the
  keyboard. It exercises the parts of the engine the platformer does not:
  high entity churn against the deferred lifecycle, one-shot (non-looping)
  animations, and hand-rolled AABB collision — `CollisionSystem` is
  deliberately unregistered, since it kills *both* entities on contact and
  offers no hook for scoring. `README.md` in that directory explains each
  choice.

- **Artwork credit and licensing.** The shooter's sprites come from Ari
  Feldman's **SpriteLib**, which is **CPL-1.0**, not public domain — its terms
  travel with the files and differ from this repository's WTFPL. The license
  is kept beside the artwork in `examples/shooter/assets/license.rtf` and must
  stay with it. The source sheet is not usable as shipped (33px pitch with 1px
  separators, no alpha), so a prepared sheet plus an `assets/SHEET.md` cell
  index are included. The README gained an **Artwork** section covering this
  and the platformer's CC0 tileset.

- **`.hermes/environment.json`** defines the project's verification recipe —
  engine build, the 319-case spec suite, and an example build as a link
  canary — so `hermes verify` gates a coding agent's "done" claim on something
  other than its own say-so.

### Changed

- **`.claude/SKILL.md` gained compile-verified genre sections** (platformer,
  shooter, JRPG, netplay, puzzle) plus a new-game scaffold, an observed
  compile-error table, and an evaluation harness under `.claude/references/`.
  Every code block in the genre sections was built before being written down.

## [1.2.5] — 2026-08-09

Documentation-only release — `README.md` had drifted from the codebase in four places. No engine source changed.

### Fixed

- **The README still advertised v1.2.0 as the current release.** Now says v1.2.4, matching the `v1.2.4` tag.
- **The run instructions named a nonexistent `checkers` example.** The directory and binary are `netplay-checkers`; the swap list now points at it.
- **The features list omitted examples.** netchat, netrepl, netplay-checkers and the Switch platformer are now listed alongside the desktop games.

### Added

- **The Windows section now documents the MinGW-w64 cross-build.** `make -f Makefile.win` produces `build/win/libstormenginev2.dll` and the spec suite (`tests.exe`, runnable under Wine); SDL2 and its satellites are cross-built from the vendored Android sources. The section previously claimed native Windows builds were unsupported. The examples are not wired into the Windows build yet.

## [1.2.4] — 2026-08-09

Correctness pass over the networking handshake, the editor and four examples, from a review of `main`. One wire-format change, described under **Changed**.

### Fixed

- **A single lost ACCEPT killed a join permanently.** ACCEPT is the last datagram of the handshake and nothing acknowledges it, so the client re-sends CONNECT_READY until one lands — but the server answered only the first, and its own step-2 retry was unreachable (`step = 2` and `online = true` are set together, so a step-2 slot always took the online branch of `Update`). The client sat re-sending into silence until it timed out, while the server had already counted it connected and fired `onConnect_`. Repeated CONNECT_READY is now answered with a fresh ACCEPT.
- **`onConnect_` could fire twice for one `clientId` with no disconnect between.** A CONNECT for a slot that was already online rotated the nonce and reset `step` to 1 without clearing `online`, so the following CONNECT_READY ran the accept path a second time. In `netplay-checkers` that seated the same player twice and consumed the seat the real second player needed. CONNECT arrives before any nonce is agreed and so cannot be authenticated; an online slot is now left strictly alone.
- **One spoofed datagram could kick any connected player.** `kNetControlClose` was honoured on a source-address match alone, with no cookie check, while `kNetControlConnectReady` seventeen lines above compared both nonces. Everything the cookie handshake protected on the way in was unprotected on the way out.
- **Opening an editor project merged it into the open one and corrupted the save.** `LoadProject` only ever created entities — nothing killed the tiles already in the world, and the tileset vectors were appended to without a clear. Both projects rendered on top of each other, and the next Save walked the `"tiles"` group and wrote every tile from both into whichever `.map` was open, with the other project's tileset entries added to its `.lua`. `CreateNewCanvas` was the only path that cleared, and Open does not go through it. The undo history is still not cleared on Open — it holds commands naming entity ids from the previous project.
- **The editor logged `"Tile ID: " + mTileId`** — pointer arithmetic on a `const char[]`, not concatenation. Once tile ids passed the literal's length the result pointed past the array and `std::string`'s constructor scanned arbitrary rodata for a NUL: garbage in the log, then a segfault once the scan left the mapped page. Three sites.
- **The editor died by SIGFPE on a tile with zero animation frames.** Its `AnimationSystem` shadows the engine's and had dropped the `numFrames <= 0` guard, while the animation panel writes the value straight through. The unsaved map went with it.
- **`RemoveTileCommand::Redo` guarded on the wrong sentinel** — `mTileId == 0` against an unset value of `(size_t)-1`, so it never caught the case it was written for and instead skipped the tile that genuinely had id 0.
- **The hockey example built its entire scene twice.** `PlayState`'s constructor called `onEnter()` and `changeState` called it again, so every entity, both `Tag("player")` calls and two `TTF_OpenFont`s ran twice. The four heap `Entity` handles and the first font leaked, and a frozen duplicate player, goalie and puck sat on the rink for the whole match.
- **The shooter's missing-player guard threw in exactly the case it guarded.** `EntityHasTag(GetEntityByTag("player"), "player")` evaluates the throwing lookup as its own argument. It uses `DoesTagExist` now, and `SpawnBullet`'s unguarded lookup is guarded too.
- **Every tile in the tanks example sampled the wrong tileset cell.** The `SpriteComponent` call omitted the `isFixed` argument, so `srcX` bound to the bool and `srcY` to `srcRectX`. The level rendered as a strip of the tileset's top row, and every tile with a non-zero `srcX` was silently marked screen-fixed.
- **`netrepl` encoded a new client's first delta against the previous occupant's snapshot.** Slot ids are recycled and the per-client base was never reset, so the joiner decoded per-tick diffs as absolute positions — all six players bunched into the corner, permanently, because the bases stayed diverged.

### Changed

- **`kNetControlClose` now carries the sender's cookie pair ahead of the reason string.** This is what authenticates a disconnect. Slots that are not yet online stay exempt: they hold no game state, and a client aborting before CONNECT_ACCEPT has no server nonce to quote — making it wait out the handshake timeout instead would earn it a 60 s IP ban. A pre-1.2.3 peer's clean disconnect is no longer parsed by a current server; the connection timeout still reaps it about ten seconds later.

##  [1.2.2] — 2026-08-05

Memory-safety and correctness pass over the networking layer and the ECS, plus a Windows cross-build. No breaking changes — every addition is additive.

### Added

- `Registry::TryGetComponent<T>()` / `Entity::TryGetComponent<T>()` — return `nullptr` when the component is absent. This is the correct accessor whenever a miss is possible; `GetComponent` must return a reference and therefore cannot report one.
- `Registry::IsAlive(Entity)` and `Registry::DoesTagExist(const std::string &)` — guards for the accessors that cannot fail safely on their own.
- Windows cross-build via MinGW-w64: `Makefile.win`, `cmake/toolchain-mingw64.cmake`, `examples/examples.win.mk`. Builds `libstormenginev2.dll` and the spec suite. Not covered by CI, which builds `Dockerfile.debian` only.
- `VPadStyle` and an optional third argument to `MakeVPadLayout(w, h, style)` — the action diamond is now lettered Xbox-style by default (Y top, X left, B right, A bottom); pass `VPadStyle::Snes` for the previous arrangement. The four touch targets are in identical positions under both, so only the lettering moves; a game binding to `state.a` gets the same button under a different thumb. **Existing calls compile unchanged but move to the Xbox lettering** — pass `VPadStyle::Snes` explicitly to keep the old layout.
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
- **Android touch input responded away from where the controls were drawn.** `finger->x`/`y` are normalised over the whole drawable, letterbox bars included, but the example scaled them by the logical width directly. The logical size is a fixed 800x480 (5:3) letterboxed onto a display that is usually wider, so every touch was squashed toward the centre. Now converted with `SDL_RenderWindowToLogical`, which knows the viewport offset and scale.
- **Android movement latched on and never released.** The example accumulated touch state into its movement flags (`moveRight_ = moveRight_ || ...`), and only a `SDL_KEYUP` cleared them. Lifting a finger raises no event — it simply stops appearing in the touch list — so on a device with no keyboard the first touch in a direction pinned the player walking that way forever. Keyboard state is now held separately and the merged flags are rebuilt from scratch each frame.

### Changed

- `examples/android-platformer` now drives the engine's virtual gamepad (`<stormengine2/input/virtualGamepad.h>`) instead of the older three-zone `touchControls.h` scheme: a circular 8-way d-pad bottom-left and an Xbox-lettered action diamond bottom-right. D-pad left/right move, d-pad up and **A** jump; the controls this game does not read are drawn dimmed rather than implying an input that does nothing. The virtual gamepad shipped in 1.2.0 with specs but no consumer — this is its first.
- `examples/android-platformer` follows the phone through all four orientations, including when the system auto-rotate toggle is off. The manifest alone cannot do this: SDL calls `SDLActivity.setOrientationBis()` from native code as the window is created and overwrites `android:screenOrientation`, choosing `SCREEN_ORIENTATION_FULL_USER` for a resizable window — which honours the auto-rotate lock, so the app was pinned to the user's preferred orientation. Setting `SDL_HINT_ORIENTATIONS` does not help; the same path still resolves to `FULL_USER`. `PlatformerActivity` now overrides `setOrientationBis()` and requests `FULL_SENSOR`. Orientation stays a per-game decision, made in the game's own Activity rather than by the engine. In portrait the fixed logical size letterboxes into a band across the middle; the game is playable but small, and the controls sit inside that band because they are positioned in logical space.
- The logger no longer flushes on every line — only on errors. ECS miss diagnostics are throttled, so a game missing every frame no longer does 60 flushed writes a second.
- Removed the dead root `Makefile.nx` (it recursed into a root `Makefile` that does not exist; the working Switch path is `examples/nx-platformer/`), a stray 0-byte `kNetMaxPacketSize` file, and the tracked `editor/imgui.ini` runtime state.
- `.dockerignore` no longer lets host-built object files into the Debian image, where they could be linked stale.
- The join address in `examples/netplay-checkers/README.md` and `docs/networking.md` is a placeholder rather than a literal `192.168.1.10`, which read as something to type verbatim. Both now say to substitute the host's own LAN address (or `localhost` for two processes on one machine) and note that `Connect()` does not wait for the server: pointing a client at an address nothing is listening on succeeds, retries silently, and times out about ten seconds later with no indication the address was wrong.

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
