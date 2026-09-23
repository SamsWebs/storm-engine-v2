# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Storm! Engine v2 — a lightweight, ECS-based 2D game engine on SDL2, made for game jams and personal projects. "v2" is the engine *generation*; the release line is currently **2.3.1** (`Makefile.debian`, `VERSION ?=`). The two "2"s mean different things: the one in the name is the product generation, the one in the version is the compatibility epoch. 1.x was frozen; 2.0.0 reset that promise and spent it (see `docs/UPGRADING.md`), and 2.x is stable again. License: WTFPL. The `common/net/` module is a port of Teeworlds 0.7.5 networking (zlib).

## Build, test, run

There is **no plain `Makefile` at the repo root**. Bare `make` fails ("no makefile found") — every root invocation, including `make -n`, needs `-f Makefile.debian`. (CI works with bare `make` only because `Dockerfile.debian`'s `COPY ./Makefile.debian /opt/library/Makefile` renames it.)

Desktop deps are the apt list in README.md. The test framework (Igloo + snowhouse) is **not** an apt package and must be built from source before any test target will link — see README.md.

```bash
make -f Makefile.debian              # default goal `all`: test-target -> run-test -> target
make -f Makefile.debian target       # build ONLY ./bin/libstormenginev2.so (what CI ships)
make -f Makefile.debian test         # build ./bin/tests and run it
make -f Makefile.debian test-target  # build ./bin/tests without running
make -f Makefile.debian run-test     # run the already-built ./bin/tests (no prerequisites, no recompile)
make -f Makefile.debian clean        # rm -f ./bin/* and every *.o under the repo root
sudo make -f Makefile.debian install # .so -> /usr/local/lib, headers -> /usr/local/include/stormengine2
```

The default goal **runs the whole spec suite before building the library** — a failing spec aborts the `.so` build. Use `test` for iteration, `target` when you only want the library.

`install` also generates `$(PREFIX)/lib/pkgconfig/stormengine2.pc` from `stormengine2.pc.in` and copies `template/` to `$(PREFIX)/share/stormengine2/template`. A game built against an installed engine should use `pkg-config --cflags --libs stormengine2`: `-lstormenginev2` alone fails with `DSO missing from command line` as soon as the game calls SDL directly, because the linker will not let it borrow the engine's transitive libraries.

`install` has **no prerequisites** — it will happily install a **stale** `./bin/libstormenginev2.so` with no warning; build `target` first. It honours `DESTDIR` and `PREFIX`, wipes `$(PREFIX)/include/stormengine2` before copying (so deleted headers no longer linger), and then `find`-deletes `*.o`, `*.d` and `*.cpp` recursively from the installed tree.

### Running an example

There is no aggregate example target — `cd` into each one.

```bash
sudo make -f Makefile.debian install         # required first: examples/editor link -lstormenginev2 from /usr/local/lib
cd examples/platformer && make && make run   # `make` builds only; `make run` launches (CWD must be the example dir)
```

Binary name matches the directory name for `platformer`, `jrpg`, `netchat`, `netrepl`, `netplay-checkers`; it differs for puzzle→`tetris`, shooter→`alienattack`, sports→`hockey`, strategy→`tanks`. Use `make run` rather than guessing `./bin/<dir>`.

## Tests

Framework is **Igloo + snowhouse** (BDD `Describe`/`It`, snowhouse `Assert::That`), not gtest/Catch2. 604 tests. Specs live in `specs/` mirroring the source tree; they `#include "../common/ecs.h"` by relative path, so the suite always tests the working tree, never the installed library. `specs/main.cpp` is the sole `main()`.

**The suite covers `common/` only.** `TESTSRCS` is `find specs` plus `find common` (the two `TESTSRCS` assignments in `Makefile.debian`), so nothing under `editor/` or `examples/` is compiled into `./bin/tests` and no spec can reach it. Wiring either in is not a small job: both include the engine as `<stormengine2/...>`, which resolves to the *installed* headers rather than the working tree the specs deliberately test. Bugs in editor and example code are caught by compilation (CI builds both, see below) and by running them — not by specs.

Test sources are globbed (`find specs -name '*.cpp'` plus `find common -name '*.cpp'`) — a new `specs/<area>/<name>.spec.cpp` is picked up with zero build-file edits, and the test binary statically compiles the engine rather than linking the `.so`.

```bash
./bin/tests            # must run from the repo root
./bin/tests --help     # authoritative option list
./bin/tests --output=xunit
```

```bash
make -f Makefile.debian memcheck TARGET=./bin/tests   # valgrind; TARGET defaults to the .so, which valgrind can't run
```

Run `./bin/tests` from anywhere other than the repo root and it **segfaults** — several specs hardcode `./specs/assets/...` paths, and `common/tilemapLoader.cpp` logs the missing file and returns an empty map, which a later spec dereferences.

### Running a single test

Igloo has **no CLI filtering**. `--filter`, test names, tags — none exist; a non-flag argv is silently ignored and the full suite runs. Two real options:

1. **Rebuild with only one spec file.** The `TEST_BIN` override is mandatory so you don't clobber `./bin/tests` with a one-spec binary that later masquerades as the suite.

   ```bash
   make -f Makefile.debian test TEST_BIN=one-spec TESTSRCS="specs/main.cpp specs/systems/movement.spec.cpp $(find common -name '*.cpp')"
   make -f Makefile.debian test TEST_BIN=one-spec TESTSRCS="specs/main.cpp specs/net/netLoopback.spec.cpp $(find common -name '*.cpp')"
   ```

   The `$(find common ...)` is required: a command-line variable beats both the `:=` and the `+=` of `TESTSRCS` in `Makefile.debian`, so the engine sources must be re-supplied by hand or the link fails with undefined symbols.

2. **Source-level selection.** Change `It(...)` to `It_Only(...)` or `Describe(...)` to `Describe_Only(...)` (also `It_Skip`/`Describe_Skip`), rebuild, rerun. If any context is `_Only`, all non-`Only` contexts are skipped process-wide.

## Formatting

LLVM base, 2-space indent, 80 columns. `.clang-format` states only those three; the rest is inherited — notably `PointerAlignment: Right` (`int *p`) and `BreakBeforeBraces: Attach`.

```bash
pre-commit install                                        # one-time per clone
pre-commit run --all-files                                # scope is common|editor|examples|specs only
clang-format -i --style=file <files>
clang-format --dry-run -Werror --style=file common/ecs.h
```

Formatting is **not checked in CI** (`pr-validate.yml` runs the compat-probe check, the spec suite in Docker, and the example/editor build — no `clang-format` step). A handful of files in `common/` have historically failed it — `input/touchControls.h`, `net/{netClient,netServer,netSocket,netTypes}.h` and `xmlLoader.{h,cpp}` were the list as of 1.3.0. Do not trust that list; check it:

```bash
clang-format --dry-run -Werror --style=file $(git ls-files 'common/**/*.h' 'common/**/*.cpp' 'common/*.h' 'common/*.cpp')
```

`pre-commit run --all-files` will reformat whatever is failing, unrelated to your change - format only the files you touched.

The hook only checks files that are *in the commit*, so a `--no-verify` commit silently leaves the rest of a file's directory drifting. If you skip the hook to keep a diff readable, format the whole example in a follow-up commit.

## CI and releases

```bash
docker build -t stormenginev2-test:amd64 --platform=linux/amd64 --no-cache . -f Dockerfile.debian && docker run --rm stormenginev2-test:amd64
```

That is the spec half of the PR build gate (the `make test` comes from the image `CMD`, invisible in the workflow YAML). The other half runs `.github/scripts/ci-build-examples.sh` in the same image:

```bash
docker run --rm -i stormenginev2-test:amd64 sh -s < .github/scripts/ci-build-examples.sh
```

That builds and links the nine desktop examples and compiles `editor/` to objects — the editor cannot be linked there because it genuinely calls `NFD_*` and libnfd has no Debian package. `examples/nx-platformer` and `examples/android-platformer` are **not** covered: neither toolchain is in the image and `.dockerignore` keeps both trees out of the context, so breakage in those two still ships green. The third PR gate is a branch-name regex: `feat\W|feature/|fix/|chore/|refactor/|docs/|style/|test/|dependabot/`, unanchored and case-sensitive (lowercase prefixes only).

Releases come from a **deliberately pushed tag**, never from a merge. `build-and-release.yml` triggers on `v*.*.*` (plus a `workflow_dispatch` button) and builds amd64 + arm64 `.deb`s from that tag.

It used to also fire on pushes to `main`, bumping the patch from tag history. That made every merge a release and derived the number from tags rather than from what the tree says it is — so merging a set of BREAKING changes published them as a patch. It happened: v1.3.2 was auto-created over the 2.0.0 wave-one merge and had to be deleted before anyone fetched it. Don't put it back.

The version in the tree is bumped by hand in `Makefile.debian`, `Makefile.win` and `web/index.html` (8 occurrences there, including the script's `BAKED` fallback), normally as its own `chore/bump-x.y.z` PR before the tag.

```bash
git tag v2.2.0 && git push origin v2.2.0
```

## Build system

Most of this is invisible from any single makefile.

`base.mk` is included by exactly three entry points — `Makefile.debian`, `examples/examples.mk` and `editor/Makefile` — and supplies CC, LIB, EDITOR_LIB, INCLUDE, CCFLAGS, the `%.o` rule, `clean`, and `memcheck`. It is Linux-desktop only; `Makefile.win` does **not** include it and keeps its own flags. Desktop leaf makefiles are 2-3 lines. The Switch build (`examples/nx-platformer/Makefile`, which includes `$(DEVKITPRO)/libnx/switch_rules`) and the Android build (`examples/android-platformer/app/jni/CMakeLists.txt`) do **not** include it and share none of its flags.

- **`clean` has repo-wide blast radius.** `ROOT_DIR` derives from `base.mk`'s own realpath, so `cd examples/puzzle && make clean` deletes every `*.o` in the repository except under `.git/`, any directory named `build/`, any directory named `.cxx/`, and `vendor/android/`, which `base.mk`'s `find ... -prune` skips. Building an example no longer triggers it — `examples.mk:22` is `all: $(TARGET)` — but running `make clean` from anywhere still sweeps the whole repo. `editor/Makefile` deliberately overrides `clean` to scope it to `editor/` (make prints an "overriding recipe" warning — that's expected).
- **Header dependency tracking exists.** `base.mk`'s `CCFLAGS` carry `-MMD -MP` and the generated `.d` files are `-include`d, so editing a header rebuilds every object that includes it. This is why no `all` starts with `clean` any more — `Makefile.debian:15` is `all: test-target run-test target`, and `examples/examples.mk:22` and `editor/Makefile:15` are both `all: $(TARGET)`. **One gap survives:** `-MMD` (not `-MD`) omits *system* headers, and `/usr/local/include` is a system directory here — so no `.d` file names an installed engine header. After `make install`, a game does **not** rebuild against the new headers on its own. `make clean` there, or you get silent ODR corruption.
- **`-lnfd` is editor-only.** It lives in `base.mk`'s `EDITOR_LIB`, not the shared `LIB`, so only `editor/Makefile` links it. It used to sit in `LIB` with a `filter-out` in `Makefile.debian`, which made every example fail with `cannot find -lnfd` on any machine without libnfd; that is fixed. `vendor/nfd` is headers only and libnfd is still not in the README's apt list.
- **GTK3 is linked unconditionally**, even for the headless console networking examples — `pkg-config gtk+-3.0` must resolve or nothing compiles. **Lua is not linked at all any more**: nothing in `common/`, `specs/` or `editor/` includes a Lua header, the editor parses its `.lua` project files by hand, and `-llua` broke every from-source build on Debian/Ubuntu (they ship `liblua5.4.so` with no versionless symlink).
- **`PROFILE=release` swaps `-O0 -g` for `-O2`.** The default is the debug profile. The flags are hashed into `.build-flags` and every object depends on that stamp, so switching profiles rebuilds rather than silently mixing `-O0` and `-O2` objects. `OPT` and `DEBUGFLAGS` can also be set on their own.
- **`cd editor && make` launches the editor** — the link rule's second command is `/usr/bin/time ... $(TARGET)`, which executes the binary. In `examples.mk` the same-looking `time` wraps `g++` instead, so examples do *not* auto-launch (several example READMEs claim otherwise; they're wrong).
- **The only root makefiles are `Makefile.debian` and `Makefile.win`.** The dead root `Makefile.nx` is gone. The working Switch path is `examples/nx-platformer/`; the MinGW cross-build is `Makefile.win` plus `examples/examples.win.mk` and `cmake/toolchain-mingw64.cmake`. `base.mk` is Linux-desktop only and deliberately carries no Windows branch — change Windows flags in `Makefile.win`.
- **Local `install` and the `.deb` are the same code path now.** `.github/workflows/build-and-release.yml` builds with `make PROFILE=release target`, strips, then runs `make install DESTDIR=<staging>` rather than re-implementing the copy, so the packaged tree and a from-source install cannot drift.

## How a game consumes the engine

Two modes exist in-repo. There is no `external/storm-engine-v2` submodule pattern here (that convention lives in downstream game repos).

1. **Installed shared library** — all desktop examples and the editor. Headers resolve via `-I/usr/local/include`, the link is `-lstormenginev2`, and `base.mk` bakes `-Wl,-rpath=/usr/local/lib` so binaries run without `LD_LIBRARY_PATH`. On Linux the engine is only ever a `.so`, never a static `.a`. The MinGW build is the exception in form only: `Makefile.win` produces a DLL plus the `libstormenginev2.dll.a` import library the linker needs.
2. **Engine as source** — `examples/nx-platformer` and `examples/android-platformer` compile `common/*.cpp` directly into the game and use a committed symlink `include/stormengine2 -> ../../../common` so `<stormengine2/...>` resolves identically. Invisible in a plain `find`; use `ls -la`.

Games always include the engine with angle brackets (`#include <stormengine2/ecs.h>`); quoted/relative includes are reserved for the game's own headers.

Consequence of the two modes: editing `common/` changes desktop builds only after `make install`, but changes Switch/Android builds immediately.

**Only the Switch build globs non-recursively.** `examples/nx-platformer/Makefile:8` lists `include/stormengine2` (a symlink to `common/`) in `SOURCES` and expands it with `$(wildcard $(CURDIR)/$(dir)/*.cpp)`, one level deep — so `common/net/` is silently absent from Switch. Android is **recursive**: `examples/android-platformer/app/jni/CMakeLists.txt:53` is `file(GLOB_RECURSE ENGINE_SRC "${REPO_ROOT}/common/*.cpp")`, so it does compile `common/net/`. A new `.cpp` in a subdirectory of `common/` therefore vanishes on Switch only.

Canonical game layout: `Makefile` (`NAME = <binname>` + `include ../examples.mk`), `assets/`, `bin/`, `src/{main.cpp, game.h, game.cpp, states/playState.{h,cpp}}`, optional `src/components/*.h` and `src/systems/*.h`.

## Platforms

Platform is chosen by **which build file you invoke**, not a `PLATFORM=` variable. There is no `DEBUG` variable; `PREFIX` and `DESTDIR` exist but only for `install`.

- Debian: `make -f Makefile.debian` — g++, `-std=c++17`. The default profile is `-O0 -g`; `PROFILE=release` swaps that for `-O2` and no `-g`, which is what the release workflow passes, so the shipped `.deb` is no longer an `-O0` build.
- Switch: devkitPro make in `examples/nx-platformer` — `-D__SWITCH__`, `-O2`, and `-fno-exceptions`. `DEVKITPRO` must be exported or the makefile hard-errors before any target is considered (including `clean` and `run`).
- Android: Gradle + NDK CMake in `examples/android-platformer`.

`-fno-exceptions` on Switch matters: the ECS still throws through `std::map::at` (`Registry::GetSystem`, `GetEntityByTag`), which becomes `abort` there rather than a catchable error. Guard with `HasSystem()` / `DoesTagExist()` instead of relying on a catch. The component cap no longer throws — `EcsComponentIdIsValid` gates every id, and a system handed a type past the cap is **latched off** (`System::disabled`) so it matches nothing, rather than ending up with an empty signature that matches everything.

Asset paths are CWD-relative string literals in each game's PlayState — there is no asset-root abstraction (`DATA_PREFIX` is defined in the makefiles but dead). Desktop binaries must run from the example directory; Switch swaps the root at compile time via `#ifdef __SWITCH__` + romfs; Android extracts APK assets in Java then `chdir()`s from C++ so the same literals work.

```bash
export DEVKITPRO=/opt/devkitpro && cd examples/nx-platformer && make   # also: make run EMULATOR=<path>, make clean
cd examples/android-platformer && ./gradlew assembleDebug installDebug
```

Toolchain install (devkitPro packages, `sdkmanager`, submodule init, `adb logcat` filters) is in README.md.

All six git submodules are Android-only third-party deps under `vendor/android/` (SDL2 2.30.11, SDL_image 2.8.8, SDL_ttf 2.22.0, SDL_mixer 2.8.1, tinyxml2 10.0.0, glm 1.0.1) — a bare clone builds and tests fine without them. SDL2/SDL_image must stay SHARED because SDLActivity `dlopen`s them by name. Two submodule *names* don't match their *paths* (`vendor/android/SDL` → `vendor/android/SDL2`, `vendor/android/SDL_image` → `vendor/android/SDL_image2`).

`vendor/` is dual-purpose: `vendor/{imgui,sol,lua,fakeit,nfd}` are checked-in headers reached via `-I$(ROOT_DIR)/vendor` for desktop; `vendor/android/*` are submodules used only by Gradle/CMake, and are skipped by `clean`'s `*.o` sweep and excluded from the Docker context (`.dockerignore`).

## Namespace

**Every engine type lives in `namespace storm`** as of 2.0.0 — `storm::Entity`, `storm::Registry`, `storm::ContactSystem`, the components, the systems, the net types, `FPS`, `MAX_COMPONENTS`, all of it. That was the largest break in 2.0.0 and it is the first thing to get right when reading older code or older docs.

`<stormengine2/compat/global.h>` is the bridge: one `using storm::X;` per public name, pulling them all back into the global namespace so a 1.x game compiles unchanged. It is a bridge, not an API — a game that keeps it forever gains nothing from the change, and a future major removes it. Use it to get green, then drop it and qualify the names.

`specs/compat/bridgedNames.h` is **generated** from the engine headers by `scripts/generate-compat-probes.py` (currently 140 names) and compiled by `specs/compat/global.spec.cpp`, so a public name added to the engine and not to the bridge fails the build. CI runs the generator with `--check`. Run it after adding any public name.

## ECS model

`common/ecs.h` + `common/ecs.cpp`. The traps here require reading the template bodies in the header against the single call site in the `.cpp`.

- `Entity` is a 24-byte value type (pinned in `specs/layout.spec.cpp`): `std::size_t id`, a `std::uint32_t generation`, and a raw `Registry *registry` back-pointer, copied by value into every system's entity vector. The generation arrived in 2.0.0 and is what stops a stale handle from naming whichever live entity recycled its id — `operator==` compares both halves. Generation 0 is reserved and never valid; `Registry::generations` starts at 1.
- **A hand-built `Entity` is inert, not UB.** `registry` is `= nullptr` in the declaration and only `Registry::CreateEntity()` sets it, and every forwarder null-checks and no-ops. `Entity(std::size_t)` is `explicit` as of 2.0.0, so a bare integer no longer converts to one.
- Component storage is **not a sparse set**. It's one dense `std::vector<T>` per type, indexed directly by entity id. Memory per registered component type is O(highest entity id), and every component type must be **default-constructible** (`Pool(int size = 100)` default-constructs 100 objects up front).
- `MAX_COMPONENTS = 64` (`common/ecs.h`, raised from 32 in 2.0.0) is a **process-wide** cap, not per-registry: `IComponent::nextId` is a single static. A 65th component type anywhere in the binary overflows it. A system that requires a type past the cap drops the requirement and ends up with an EMPTY signature, which matches *every* entity — `KNOWN_ISSUES.md` item 4.
- **System membership is computed exactly once per entity**, when `Registry::Update()` flushes `entitiesToBeAdded`. `AddComponent`/`RemoveComponent` only flip signature bits — they never re-evaluate system membership. Adding a component to a live entity will not get it into a matching system; removing one will not take it out. This is the single biggest correctness trap in the ECS (`KNOWN_ISSUES.md` item 5). Kill-and-recreate, or drive `Registry::RemoveEntityFromSystems` / `AddEntityToSystems` by hand — that pair is the retrofit for a system registered late, and `specs/systemMembership.spec.cpp` covers it.
- **Two systems sidestep that, and only those two.** `ContactSystem` and `RenderColliderSystem` require `TransformComponent` **alone** and narrow to the entities carrying a collider inside their own `Update()`, so a box or circle collider added to (or removed from) a live entity does take effect. A signature is an AND, so neither could express "a box collider OR a circle collider" any other way. The cost is that `GetSystemEntities()` on both reports transform entities rather than bodies, and both scan every transform entity per frame.
- Entity creation and destruction are **deferred**. Call `registry.Update()` at the start of `update()`, before any system runs, or new entities are invisible for a frame.
- **There is no system scheduler and no virtual `System::Update`.** Each concrete system declares its own non-virtual `Update` with a bespoke signature (`Update(double dt)`, `Update()`, `Update(SDL_Renderer*, const AssetStore&, const SDL_Rect* camera = nullptr)`, …) and the game state calls each by name in an order it chooses.
- **`AddSystem<T>()` only constructs and registers the system** — it never touches entities. Entities are routed solely by `Registry::Update()` → `AddEntityToSystems()`, so a system registered after entities were already flushed starts empty and stays empty.
- Canonical per-tick order (from the examples): `registry_.Update()` first, then simulation systems (movement → animation → contacts) in `update()`, then render systems in `render()`.
- **`ContactSystem` is the only collision system.** `common/systems/contact.h` detects overlaps and *reports* them — `GetContacts()` gives `Contact{a, b, normal, depth}` with `a` always the lower entity id, plus `SetOnBeginContact`/`SetOnEndContact` for once-per-pair transitions, `SetPairFilter` for layers and sensors, and `MinimumTranslation` for depenetration. It never kills, moves or writes anything; response is the game's call, since there is no scheduler. `CollisionSystem` — the 1.0-era kill-both-on-contact system — was **deleted in 2.0.0**; `common/systems/collision.h` no longer exists, and code calling `AddSystem<CollisionSystem>()` does not compile. Overlap is **strict** throughout: a shared edge is a zero-area overlap with no meaningful normal, so it is not a contact.
- **Bodies are boxes or circles.** `BoxColliderComponent` and `CircleColliderComponent`; give an entity one or the other, never both (both is a game bug, and the box wins). A circle's `offset` places the **centre**, unlike a box's top-left, and `transform.scale` scales its radius by the larger absolute axis — a circle cannot be an ellipse. Against a box corner a circle reports a diagonal normal where a box snaps to an axis, which is the whole reason it exists. The math is free-standing in `common/collision/shapes.h` (glm and nothing else from the engine): `Overlaps` / `Manifold` / `ClosestPointOn` / `MinimumTranslation` / `BoundsOf`, overloaded for all three pairings and usable with no `Registry` at all.
- **`RenderColliderSystem` is the debug overlay**, drawing both shapes in green and resolving them through `ContactSystem::BoundsOf` / `CircleOf` so it cannot disagree with the sweep. `Update(renderer, &camera)` pans like `RenderSystem`; the camera is optional and there is no `isFixed` equivalent, because a collider is always a world body.
- **The broadphase is a uniform grid**, no preferred axis — a column of platforms costs the same as a row. Cell size is derived per frame from the bodies present (twice their mean extent); `SetCellSize(float)` overrides it and 0 restores the derived value. A body far larger than the grid — a level-sized floor collider — is tested against everything instead of filling thousands of cells. Do not build tilemap collision from one collider entity per tile; the per-box least-penetration manifold catches on seams. Snap to the grid like `examples/platformer`.
- **`AssetStore` caches fonts and sounds too** (1.3.0): `AddFont(id, path, ptSize)` / `GetFont`, `AddSound(id, path)` / `GetSound`, and `ClearAssets()` frees all three. A `TTF_Font` is one point size, so register one id per size (`"hud-18"`). **`ClearAssets()` must run before `TTF_Quit()` / `Mix_CloseAudio()` / `SDL_Quit()`** - those free every font and chunk themselves, and the store usually outlives the state that shut them down. Three examples had that order wrong.
- **`common/text.h`** - `Text::Draw` / `DrawCentred` / `Measure`. Header-only, null-safe on renderer and font, leaks nothing on any failure path. Four examples had hand-rolled copies; one re-opened the font from disk on every call.
- **`common/input/gamepad.h`** - `Gamepad` over `SDL_GameController`: `Down(GamepadButton)`, `Pressed(...)`, `Released(...)`, `Current()` for analog sticks and triggers. Not the same thing as `input/virtualGamepad.h`, which is an on-screen touch pad and is SDL-free. Call `Shutdown()` before `SDL_Quit()`.
- **`common/states/gameStateBase.h`** (1.3.0) - the `GameState` interface with none of the convenience includes: 80,265 preprocessed lines against `gameState.h`'s 146,748, a 45% saving. `gameState.h` includes it and adds the rest, so nothing existing changed. `gameStateMachine.h` includes the *base*, deliberately - it uses only `GameState *`, and including the convenience header would drag the whole engine back into every state that switches states, making the slim header pointless. `specs/gameStateMachineSlim.spec.cpp` guards that at compile time by defining its own `Registry`, which collides if `ecs.h` leaks back in. A game that does not use the ECS should include the base header and include what it uses.
- **`GameState::CapFrameRate(maxDeltaSeconds = 0.05)`** - paces the frame, returns elapsed seconds, rolls `millisecondsPreviousFrame` forward. Pass 0 to leave the delta unclamped. Seven states used to write this out by hand and five shadowed the base member to do it. Non-virtual and adds no member, so `GameState`'s layout and vtable are unchanged.
- `Registry::AddEntityToSystem(Entity)` (singular) was **declared with no definition anywhere** on pre-1.2.x sources — a link error. It is gone; `common/ecs.h` carries a comment where it used to be. The real entry point is `AddEntityToSystems`. (`System::AddEntityToSystem` is a different, defined method.)
- Two usage styles coexist: the **editor** uses the `Registry::Instance()` singleton; every **game** owns a plain `Registry registry_` member per game state, so each state is its own world.
- The `Logger` writes to `std::cout` on every entity creation and every component add, and keeps a process-wide static history capped at 1000 entries. ECS-heavy frames do synchronous console I/O.

## Game loop and state machine

The engine ships **no main loop, no Game class, no window management**. `Game::Run()` is written by the game: `Initialize(); while (isRunning) { ProcessInput(); Update(); Render(); }`. The only engine piece in it is `GameStateMachine`, which forwards each phase to the top state. A state stops the loop by writing to a `bool&` the Game handed to its constructor — there is no engine quit API.

Timestep is variable dt with a 60 FPS **cap**: each state computes `MILLISECS_PER_FRAME - elapsed` and `SDL_Delay`s the remainder of a ~16 ms budget. Nothing enforces a minimum frame rate. `FPS` / `MILLISECS_PER_FRAME` are declared in `common/states/gameState.h` but used only by game code, never by the engine. Games typically stack two throttles: `SDL_RENDERER_PRESENTVSYNC` *and* the state's own delay budget; disabling one does not uncap.

`GameStateMachine` (`common/gameStateMachine.{h,cpp}`) **owns every state pointer** — pass `new`-allocated states and never delete them yourself. Deletion of a discarded state is **deferred to the next tick**: `popState`/`changeState` move the old state into `m_defunctStates`, and `sweepDefunct()` frees it at the top of the next `processInput()` or `update()`. Rationale: those calls normally come from inside the discarded state's own method, so an inline delete would free the caller's `this` mid-call (the 1.0.1 use-after-free).

Consequences to code against:

- **`render()` does not sweep.** A state discarded during `update()` stays allocated (already popped, already `onExit()`-ed) through the following `render()`, and is freed at the top of the next `processInput()`.
- **Return immediately** after calling `changeState`/`popState` on yourself — your object is already off the stack and `onExit()` has already run.
- **`onExit()` must be idempotent** — it can run twice (machine call + destructor).
- `changeState` to a state with the **same `getStateID()`** is a no-op that deletes the *rejected* new state **inline**, not deferred. Handing the machine a duplicate-id state transfers ownership and frees it immediately.
- Initialize in `onEnter()`, tear down in `onExit()` — not the ctor/dtor. `changeState` calls `onEnter()` after pushing; `clean()` calls `onExit()` before deleting. The `netplay-checkers` example follows this; `platformer` does the opposite (setup in the ctor), so don't take one example as the rule.

Input: `common/input/` is five header-only files — `keyboard.h` (`Keyboard`, edge-triggered key state, does not poll), `gamepad.h` (`Gamepad` over `SDL_GameController`), `actionMap.h` (`ActionMap`, one named action across keyboard, gamepad, virtual pad and touch), and the two SDL-free touch headers `touchControls.h` and `virtualGamepad.h`. All header-only, so they contribute nothing to the `.so`. None of them polls: quit handling and the `SDL_PollEvent` loop still live in each state's `processInput()`. **Never call `SDL_PollEvent` in both `Game::ProcessInput` and the state's `processInput`** — the queue is shared, whoever polls first consumes everything, and the other silently sees no input. The active state owns all polling. The v1.2.0 virtual gamepad has no in-repo consumer besides its spec; `android-platformer` still uses the simpler `touchControls.h` three-zone scheme.

`common/states/gameState.h` transitively includes SDL2 plus all components, all systems, the AssetStore, Logger and TileMapLoader — every TU touching a state header compiles the whole engine surface.

`GameState`'s protected `millisecondsPreviousFrame` is used directly by `puzzle` (`examples/puzzle/src/states/playState.cpp:292`) and `strategy` (`examples/strategy/src/states/playState.cpp:147`); `platformer`, `shooter`, `jrpg`, `nx-platformer` and `android-platformer` shadow it with their own `millisecondsPreviousFrame_`. Both conventions are in-tree — check which one a state uses before touching it.

`AssetStore::GetTexture` returns `nullptr` for a missing id (it no longer throws). Null-check it.

## Networking (`common/net/`)

Hand-rolled non-blocking UDP over raw BSD/winsock sockets — **no SDL_net, no enet, no TCP**. Strictly client–server / host-authoritative, IPv4 only, no P2P, no NAT traversal, no matchmaking. The server slot array is fixed at 16 (`NetServer::kMaxClients`) but `Start(uint16_t port, int maxClients = 8)` defaults to **8** — pass 16 explicitly if you want all of them (the running server logs its `maxClients_`, not `kMaxClients`). Ported from Teeworlds 0.7.5: 10-bit sequence/ack window, receiver-driven resend requests, cookie handshake, snapshot deltas.

Game-facing surface is one header, `<stormengine2/net/net.h>`: `NetServer`, `NetClient`, `NetSnapshot`/`NetSnapshotDelta`/`NetSnapshotCache`, `NetMessageWriter`/`NetMessageReader`.

**Zero coupling to the ECS or the engine tick.** `grep -rn "Registry|ecs.h|SDL" common/net/` matches only the words "SDL-free" in comments. Snapshots are flat arrays of `(uint16 type, uint16 id, int32[] payload)` keyed `(type << 16) | id`; the game hand-marshals ECS components in and out. Nothing in `net/` drives a tick — each net example paces itself (netrepl: `NetNowMs()` + `usleep` at 60 Hz; netplay-checkers: vsync only).

Rules you can only learn by tracing multiple files:

- **Call `Update()` before `Poll()` every frame.** `NetConnection` caches the clock in `nowMs_`, written only by `Update(nowMs)`. `Feed()`/`Flush()` (driven by `Poll`) read that cached value for RTT, `lastRecvMs_`, `lastSendMs_`. A Poll-only loop has a frozen clock: RTT reads 0 and timeouts never fire.
- **`NetChunk::data` points into per-connection scratch that the next `Feed()` overwrites** — i.e. the next datagram inside the *same* `Poll()` loop. Copy anything you need past the callback.
- **Single-threaded and lock-free by construction.** No `<thread>`/`<mutex>`/`<atomic>` anywhere. All callbacks fire synchronously on the thread calling `Poll()`/`Update()`. `NetRandom32()` uses unsynchronized function-local statics.
- **Every vital `Send()` is its own datagram** — `Send`/`Broadcast` queue then flush immediately. Only non-vital chunks batch (flushed once at end of `Poll()`). Broadcasting N reliable messages per tick costs N datagrams per client.
- **Overflowing the unacked-vital window kills the connection**, it does not block or drop. 96 entries or 16 KB, whichever hits first; a chunk unacked for 10 s is also fatal. Both call `SetError("too weak connection...")` and the next `Update()` drops the client.
- **Ordering:** vital chunks are lossless and in-order, implemented by *discard + request resend*, not a reorder buffer. Non-vital may be lost or reordered. Acks only piggyback on the receiver's own outgoing traffic.
- **Hard ceilings:** chunk ≤ 1200 bytes, datagram ≤ 1400 (no fragmentation layer), snapshot ≤ 256 items / 2048 int32s, prediction cache = 16 ticks (~267 ms at 60 Hz). Oversize `Send` just returns `false` with no message.
- **`NetSnapshot` is two-phase:** `AddItem` only before `Finish()`, `FindItem`/`GetItemByIndex` only after. An in-place `AddItem` replace requires an identical field count. Even an empty delta base must have `Finish()` called on it — otherwise `FindItem` silently fails and every tick re-encodes the whole world as new items.
- **A stalled handshake auto-bans the IP for 60 s** (CONNECT seen, CONNECT_READY never proven, 10 s timeout). Per-IP concurrent slots cap at 4, which limits local multi-client testing.
- `NetServer`/`NetClient` install send lambdas capturing `this` (and a slot index), so they must never be copied — and as of 2.0.0 they cannot be: `NetServer`, `NetClient`, `NetConnection` and `NetSocket` all `= delete` their copy constructor and copy assignment (`KNOWN_ISSUES.md` item 6, fixed). Hold them by reference or `unique_ptr`, never by value and never in a resizing `std::vector`; both are also far too large for the stack (~372 KB and ~188 KB). Their destructors fire user callbacks, so explicitly `Stop()`/`Disconnect()` in `onExit()` before teardown.
- Disconnect reasons are bare string literals with no enum, scattered across `netConnection.cpp` / `netServer.cpp` / `netClient.cpp` (`"timeout"`, `"server full"`, `"banned"`, …). Don't switch on them; if you must, grep all three files — the set is not centralized.

Reference implementations, in increasing order of realism: `netchat` (minimal console host/join + reliable echo), `netrepl` (60 Hz authoritative host, per-client base snapshot, delta encode/apply — note bases advance on *send*, not ack, so the delta must be vital), `netplay-checkers` (graphical, ECS, but uses **full-state broadcast, not snapshots** — the right call for turn-based, and it doubles as late-joiner sync). `specs/net/netLoopback.spec.cpp`'s `PumpUntil` helper is the canonical verified pattern for driving both sides.

`docs/networking.md` and `netrepl` both use a C99 VLA for the delta buffer — a GCC extension, fine under `g++` here, not portable.

```bash
cd examples/netchat && make && ./bin/netchat host 5000
./bin/netchat join 127.0.0.1 5000 alice
cd examples/netrepl && make && ./bin/netrepl host 5000
./bin/netrepl join 127.0.0.1 5000
cd examples/netplay-checkers && make && ./bin/netplay-checkers host 51235
./bin/netplay-checkers 192.168.1.10 51235   # join: IP as argv[1], NO 'join' keyword — asymmetric with the others
```

## Editor and tilemaps

```bash
cd editor && make        # builds AND launches
cd editor && make run    # run the already-built binary
cd editor && make clean
```

The editor is a standalone SDL2 + ImGui + sol2/Lua tilemap/collider painter that *links* the engine and reuses its `Registry::Instance()`, Logger and component structs — it is not an engine subsystem. It also declares its own `RenderSystem`/`AnimationSystem` under `editor/src/rendering/` that shadow the engine's same-named systems. It must run with CWD=`editor/` (it reads `fonts/fontawesome-webfont.ttf` and `./assets/mouse_hand.png`; `editor/README.md`'s claim that assets live in `bin/assets/` is stale).

It writes three files: `<name>.lua` (project: canvas size, tile size, tileset id→path), `<name>.map` (tiles), `<name>_colliders.map`. `TileMapLoader` reads the `.map` and auto-detects format by peeking the first non-space char — alpha means editor format, digit means legacy CSV. That flips the meaning of the constructor's second argument: editor maps embed srcX/srcY so you pass `""`; legacy CSV needs the PNG (only `examples/strategy` still does this). **No example ever reads the `.lua` file** — it's an editor project file, despite sitting next to the `.map` in every `assets/tilemaps/`.

## Conventions

### Naming and ownership

Three member-naming schemes coexist and are load-bearing: engine ECS core uses bare names (`numEntities`), `GameStateMachine` uses `m_` (`m_gameStates`), all game/state code uses a trailing underscore (`renderer_`). Method casing is split too: PascalCase in ECS/AssetStore/Logger (`CreateEntity`, `AddTexture`), camelCase in the state machine and `GameState` virtuals (`pushState`, `processInput`, `onEnter`) — overrides must match camelCase.

Ownership is narrow and consistent: raw *owning* pointers exist only for `GameState*` (the machine owns them, which is exactly what the deferred-delete machinery protects). Everything else is `unique_ptr` (`AssetStore_Ptr`, `Logger_Ptr`) or `shared_ptr` (Registry pools and systems). SDL handles are raw non-owning, destroyed manually in `Game::Destroy` and `AssetStore::ClearAssets`. Engine code throws nothing itself; only std `.at()` can throw.

### Repo hygiene

- `.editorconfig` sets `indent_style = space` for `[*]` with **no Makefile exception**; an editor obeying it literally will convert Make recipe tabs and break the build.
- `.gitignore`'s `/main` is root-anchored on purpose — an un-anchored `main` once matched the Android example's `app/src/main/` and shipped a 1.1.0 release that couldn't be built from a clean clone. Don't un-anchor it.
- No CONTRIBUTING.md, no commit-message convention. Commit style in history is imperative sentence-case with a trailing `(#N)` — not Conventional Commits, despite the branch-name check linking to conventionalcommits.org.

### Docs

README.md (build/install/platforms), TUTORIAL.md (API walkthrough), CHANGELOG.md (behavioral contracts — several lifetime rules exist *only* there), KNOWN_ISSUES.md (defects frozen behind the compatibility promise), docs/ROADMAP.md (what landed, what a review found, what is carried), docs/UPGRADING.md (the 2.0.0 breaks), `docs/networking.md`, `editor/README.md` + `editor/TUTORIAL.md` (the only walkthrough for the painter), `.claude/SKILL.md` + `.claude/references/` (what future sessions read instead of the source), and 12 per-example READMEs.

## Working style

- **Verify, don't assume.** Read the code before describing it; measure before
  claiming. If you assert a number, show where it came from.
- **Be honest about failure.** If something doesn't work, say so plainly with the
  evidence. If you were wrong earlier, correct it explicitly rather than quietly
  moving on.
- **Audit before generating.** Reading what is already here has repeatedly
  surfaced real defects that new code would never have fixed - a system with
  zero registrations, a declared-but-undefined method, two parsers for one file
  format. Scan first.
- Ask before spending money (API credits) or doing anything hard to reverse.
- Keep `CHANGELOG.md`, `KNOWN_ISSUES.md` and `docs/ROADMAP.md` current as work
  lands. `CHANGELOG.md` is not a formality here: several lifetime and ownership
  contracts are documented *only* there. (`docs/TECH_DEBT.md` is a gitignored
  local notebook; its durable open half lives under "Carried from the
  TECH_DEBT notebook" in `docs/ROADMAP.md`.)
- **A public API change is documented in the same branch that makes it.** This
  engine is consumed by games that pin it, so a new system, a new component, a
  changed signature or a changed behaviour updates `TUTORIAL.md` and
  `CHANGELOG.md` as part of the work. Verify the surrounding claims while you
  are in there; both files have carried false ones before.
- **The 2.x promise decides what a fix is allowed to be.** Changing a public
  signature, changing the layout of a type a game embeds or passes by value, or
  deleting a public member is off the table until the next major. A defect that
  can only be fixed that way belongs in `KNOWN_ISSUES.md` with its reasoning,
  not in a release — that file is scoped to break-requiring defects, and a
  non-breaking one belongs on `docs/ROADMAP.md`'s carried list instead.
  Additive changes are always available - prefer them.

### Audit before modifying

This is a mature codebase with twelve consuming examples and an editor. Assume
that requested functionality may already exist but have incomplete,
inconsistent, stale, or incorrectly connected plumbing.

**Do not write new code until the existing implementation has been thoroughly
inspected.**

Before modifying anything:

1. Search the **entire codebase** for existing implementations related to the work - `common/`, `editor/`, every directory under `examples/`, and `specs/`.
2. Identify the existing data structures, state, state transitions, functions, systems, components, serialization, and specs involved.
3. Trace **every path** that can perform, trigger, persist, or display the operation being changed.
4. Search for callers and consumers of affected functions, types and state. A system with no `AddSystem<T>()` call site anywhere is not "the way to do this" - it is dead.
5. Search for related implementations under different names. Do not assume terminology is consistent.
6. Inspect specs, fixtures, loaders and editor code for existing assumptions.
7. Do not assume something is missing because it has not been found yet. **Search for it first.**

When auditing existing work, assume the likely problem is often:

> The feature exists, but one or more paths are not wired into it correctly.

Look specifically for:

- incomplete feature plumbing
- partially wired state transitions
- alternate paths that bypass the intended mechanism
- stale callers after API changes
- serialization/deserialization mismatches between the editor's writer and the engine's reader
- events that are emitted but not consumed, or that should be emitted and are not
- duplicated logic across `common/` and a game
- dead code indicating an incomplete migration
- initialization/teardown gaps, and `onEnter`/`onExit` asymmetry
- **a comment asserting what another file does** - prose is not compiled, and several such claims in this repo were false
- **a field whose name stopped matching its meaning** - the readers that were not updated are the bug
- **a doc stating a build fact that has since changed** - `CLAUDE.md` itself has carried stale claims about header dependency tracking, linked libraries and the test count
- **a system that is registered but never receives entities** - `Registry::Update()` computes membership exactly once, when the entity is admitted
- **a header-only file that no `.cpp` pulls in** - it contributes nothing to the `.so` and no spec may be reaching it either

`KNOWN_ISSUES.md` carries the defects that are real, understood and deliberately
frozen, each with the reasoning. **Read it before an audit** - re-discovering one
of them and filing it as new is wasted work, and several are traps that bite
silently.

Pay particular attention to **integration boundaries**. A feature can be correct
in isolation while still being broken because another subsystem does not invoke
it, or because it never reaches Switch and Android at all.

### Existing plumbing takes precedence

Reuse existing plumbing wherever possible.

If existing plumbing is imperfect, **fix the existing plumbing rather than
creating a parallel mechanism**.

- Do not create a second implementation of something that already exists.
- Do not introduce parallel state when existing state can represent the requirement.
- Do not introduce a new component type when a `std::function` seam or a widened existing component serves - `MAX_COMPONENTS` is 64 per **binary**, not per Registry.
- Do not rename or replace functioning systems merely because another design appears cleaner.
- Do not modify unrelated systems.
- Only add new mechanisms when the required behavior genuinely does not exist.

Preferred order:

**existing mechanism → correct/extend existing mechanism → new mechanism only when genuinely necessary**

There is a second axis here that ordinary projects do not have: the boundary
between engine and game is movable. Generic, reusable code sitting in an example
may belong in `common/` - but promoting it costs a release, a CHANGELOG entry,
specs, and a pin bump in every consuming game, so it needs a second consumer or
a plainly engine-shaped design. Leaving it local is a legitimate answer.

When a change genuinely does need a new mechanism, the shape it takes still has
to survive the Switch and Android builds — non-recursive globs on Switch,
`-fno-exceptions` on both — neither of which is in CI.

### Implementation and verification

After implementation, do not stop because the changed code compiles or the
immediate tests pass.

Verify:

- all affected callers
- all relevant state transitions
- alternate entry points and bypass paths
- serialization/deserialization, and editor-writer against engine-reader
- error paths
- platform-specific behavior - Switch `-fno-exceptions` and the non-recursive `common/*.cpp` globs on Switch and Android, neither of which is in CI
- regression coverage: a fix is only really closed once a spec fails without it
- cross-subsystem integration, including at least one real consumer under `examples/`

Ask:

> **What path through this system could still be broken even though the obvious path now works?**

The final standard is not merely "the new code works."

The standard is:

> **There is one coherent implementation, all existing paths use the correct plumbing, state remains consistent across the complete lifecycle, and there are no known bypasses or incomplete integrations.**

## Working in this repo

### Documentation is part of the change, not a follow-up

**Before opening a pull request, update the documentation the change touches.**
A PR that changes behaviour and leaves the docs describing the old behaviour is
not finished — the next reader greps, finds the document, and believes it.

Walk this list every time and decide for each one. Most changes touch several;
almost none touch all of them. Skipping one is a decision to state in the PR,
not an omission to discover later:

| File | Update it when |
|------|----------------|
| `README.md` | The engine's surface changes — a new component or system, a changed requirement, a capability gained or lost |
| `examples/<name>/README.md` | That example changed, or the engine change alters how it behaves or is built |
| `docs/ROADMAP.md` | An item on it lands, a claim it makes stops being true, or the work leaves something carried. Record what a review found, including what was *not* fixed |
| `KNOWN_ISSUES.md` | A listed defect is fixed or changes shape, or a new one is found **whose fix needs a compatibility break**. Non-breaking defects belong on the roadmap's carried list instead — that scope rule is in the file's own header |
| `TUTORIAL.md` | A component, system, signature or convention a game author uses changes. The built-in component and system tables live here |
| `CHANGELOG.md` | Always, for anything a consumer can observe. Say what changed, why, and what it costs — including the breaks and the new per-frame work |
| `docs/networking.md` | Anything under `common/net/` changes |
| `.claude/SKILL.md` | The engine's public surface changes. This is what future sessions read instead of the source, so a stale entry here is worse than no entry. Its `references/` files count too — `compile-errors.md` in particular, which tells readers what an error means and will send them the wrong way if a signature moved |

Two things that are easy to miss:

- **Cross-references rot.** Prose in one file that describes another file's
  behaviour goes stale silently. Grep for the claim, not just the filename.
- **Cite function names, never line numbers.** `grep -n` finds a name; a line
  number is wrong by the next commit. `docs/ROADMAP.md` has an item on this with
  the history behind it.

## Before the PR

- `make -f Makefile.debian test` — the whole suite, and quote the count.
- `python3 scripts/generate-compat-probes.py` if a public name was added, then
  `--check` to confirm it matches. CI runs the check.
- New behaviour gets a spec. New public API gets a spec even when nothing in the
  engine calls it.
