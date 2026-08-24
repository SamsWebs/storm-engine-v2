# New game scaffold — Storm! Engine v2 (any 1.x)

A complete, minimum standalone game: window, state machine, ECS registry, one
animated entity, keyboard input, quit. Copy the directory, rename, build.

Every file here exists because the engine does not ship it. `Game` and the main
loop are written by the game, not the engine, so a scaffold is the only way to
avoid re-deriving ~90 lines of boilerplate per project.

Since v1.3.0 the engine also *installs* a starter game of its own at
`$(PREFIX)/share/stormengine2/template`, built with `pkg-config` and written
against 1.3.0 (`ContactSystem`, `AssetStore::AddFont`, `Text`). Start there if
you know you are on 1.3.0 and want the current idiom; start here if you need to
build against an older install, or want the annotated version.

## Layout

```
mygame/
├── Makefile              # real flags — NOT the 2-line examples/ Makefile
├── verify.sh             # build + headless run + log check
├── assets/
│   ├── README.md         # frame layout and the `vertical` flag trap
│   └── gfx/
│       ├── player.png    # 128x32, 4 horizontal frames — ships with the scaffold
│       └── tileset.png   # 128x32, 4 tiles
└── src/
    ├── main.cpp          # 8 lines: construct, Run, Destroy
    ├── game.h            # window + renderer + state machine
    ├── game.cpp          # SDL init, first state, the loop
    └── states/
        ├── playState.h
        └── playState.cpp # the golden path: systems, entities, input, render
```

## Build and run

```bash
cp -r new-game-scaffold ~/Projects/mygame
cd ~/Projects/mygame
# edit NAME in the Makefile, then:
make
./bin/mygame
```

Run from the game root — the asset path `./assets/gfx/player.png` is relative to
the working directory, not the binary.

## Engine version

The scaffold uses only the stable 1.x surface and compiles against **any 1.x
install** - verified against a pre-v1.2.2 build, 1.2.5, and 1.3.0. That
portability is the point of this directory, and it is why the scaffold still
writes out its own frame-pacing loop rather than calling the newer, shorter
engine API.

Calls it deliberately avoids, and what each would cost you:

- **`Registry::DoesTagExist`** is v1.2.2+. The scaffold holds the player in a
  `std::optional<Entity>` instead of looking it up by tag each frame. Once your
  game can *kill* the player, that cache is no longer safe — ids are recycled
  and `Entity` carries no generation — so switch to a tag lookup guarded by
  `DoesTagExist` and accept the v1.2.2 floor.
- **The camera argument to `RenderSystem::Update`** is v1.2.1+, but it is a
  defaulted parameter, so *omitting* it compiles everywhere. Passing an explicit
  `nullptr` is what breaks older headers. The moment you want scrolling you pass
  `&camera` and take the v1.2.1 floor, which is almost certainly fine.
- **`GameState::CapFrameRate()`** is v1.3.0+. One call replaces both the
  hand-written `MILLISECS_PER_FRAME - elapsed` / `SDL_Delay` block in
  `PlayState::update()` *and* the `millisecondsPreviousFrame_` member shadowing
  the inherited one, and it clamps a hitch (0.05 s by default) so a long frame
  cannot teleport everything through a wall:
  `const double deltaTime = CapFrameRate();`. Five in-repo examples made exactly
  that swap. Take it as soon as you are willing to require 1.3.0.
- **`ContactSystem`, `Text` and `Gamepad`** are all v1.3.0+. See
  **Next steps from here**.

Reaching for any 1.3.0 call raises the floor to 1.3.0, and that is a
**rebuild**, not a relink: `sizeof(AssetStore)` changed from 112 to 208 bytes in
that release, so a game compiled against 1.2.x headers running on the 1.3.0
`.so` overflows the heap with nothing warning.

If you are on an old install and want the newer API, rebuild and reinstall:

```bash
cd /path/to/storm-engine-v2 && make -f Makefile.debian target && sudo make -f Makefile.debian install
```

`install` has no prerequisites and will happily install a stale `.so`, which is
why `target` comes first.

## Why the Makefile is not two lines

`examples/platformer/Makefile` is:

```make
NAME = platformer
include ../examples.mk
```

That works only inside `examples/`, where `examples.mk` and `base.mk` are
reachable above it. A game outside the engine tree has neither, so the flags
are spelled out in this Makefile instead. Three differences worth knowing:

- **`clean` is scoped to this project.** The engine's `base.mk clean` derives
  `ROOT_DIR` from its own realpath and deletes every `*.o` in the entire engine
  repository — running it from an example wipes the other examples.
- **lua, nfd and gtk+-3.0 are not linked.** `base.mk` links them
  unconditionally, but they are editor dependencies; a game needs SDL2 and the
  engine only.
- **`-Wl,-rpath=/usr/local/lib`** lets the binary find `libstormenginev2.so` at
  run time without `LD_LIBRARY_PATH`.

## What the scaffold demonstrates

Each of these is a rule from SKILL.md that the code obeys, in place:

| In the code | Rule |
|---|---|
| `AddSystem` calls precede `SpawnPlayer` | System membership is computed once, at flush. A system registered after entities exist stays empty forever. |
| `registry_.Update()` first in `update()` | Entity creation and destruction are deferred and batched. |
| `SDL_PollEvent` only in `processInput()` | The event queue is shared; the active state owns all polling. |
| Setup in `onEnter()`, teardown in `onExit()` | `changeState` calls `onEnter` after pushing; `clean()` calls `onExit` before deleting. |
| `onExit()` is safe to run twice | It can fire from the machine and again from the destructor. |
| `isRunning_` is a `bool&` | There is no engine quit API. A state stops the loop by writing to the flag the Game handed it. |
| `GetTexture` result is null-checked | It returns `nullptr` for a missing id rather than throwing. |
| `RenderColliderSystem::Update(renderer_)` | It takes only the renderer — unlike `RenderSystem::Update`, the debug overlay is not camera-aware. |
| `std::optional<Entity>` rather than a bare member | `Entity` has no default constructor at all, only `Entity(std::size_t)`, so a bare `Entity player_;` member does not even compile. (Its `registry` pointer *is* null-initialised, and since v1.2.2 a hand-built `Entity(88)` is inert rather than UB: every forwarder null-checks and no-ops.) |

## Verification status

All three translation units compile clean with `-Wall -std=c++17` against a
pre-v1.2.2 header set, 1.2.5, and 1.3.0 - that is the evidence for the "any 1.x"
claim above. 1.3.0 only added API (`ContactSystem`, `Text`, `Gamepad`,
`AssetStore` fonts and sounds, `CapFrameRate`), so nothing here needed a
change for it.

It also builds, links and runs end to end against a current install:

```
PASS: built, linked, ran 4s, 2 texture(s), 1 entity/entities, no errors
```

Both textures load, the player entity receives all four components, the loop
survives the full window, and teardown is clean. Reproduce with `./verify.sh`.

## Next steps from here

- **Tiles** — add `TileMapLoader`, and check `getMap().empty()` before use; a
  failed load is reported through `Logger::Err` but still returns an empty map.
- **Scrolling** — build an `SDL_Rect camera` in `render()` and pass `&camera`
  to `RenderSystem::Update` instead of omitting the argument.
- **Collision** - on 1.3.0+, register `ContactSystem`
  (`<stormengine2/systems/contact.h>`). It reports overlaps with a unit normal
  and a penetration depth and never kills, moves or writes anything, so bounce,
  pickups, damage and triggers are all yours to write; `SetPairFilter` is where
  layers and sensors live. Do **not** register `CollisionSystem`: it is
  deprecated in 1.3.0 and its only possible response to an overlap is `Kill()`
  on both movable entities. On an older install, hand-roll the overlap pass.
  Either way, do tile collision against a solid-grid array, one axis at a time:
  a per-box manifold catches on the seam between adjacent tile colliders.
- **Text and fonts** - on 1.3.0+, `assetStore_->AddFont("hud-18", path, 18)`
  plus `Text::Draw` / `Text::DrawCentred` (`<stormengine2/text.h>`, not
  transitively included). Both are null-safe, so a missing font draws nothing
  rather than crashing. Call `ClearAssets()` before `TTF_Quit()`, which the
  scaffold's `onExit()` already does.
- **A gamepad** - on 1.3.0+, `<stormengine2/input/gamepad.h>`: hold a `Gamepad`
  by value in `Game`, feed it events from `processInput()` with `HandleEvent`,
  call `Update()` once after the event loop, then read
  `Down`/`Pressed`/`Released`. Call `Shutdown()` before `SDL_Quit()`.
- **Frame pacing** - on 1.3.0+, delete the `SDL_Delay` block and the
  `millisecondsPreviousFrame_` member from `PlayState` and call
  `CapFrameRate()` instead; see **Engine version** above.
- **A second state** — `pushState(new PauseState(...))` and `return`
  immediately; your object is already off the stack.

## Verifying a game actually works

`make` succeeding proves very little — a game can compile and then fail to open
a window, load no textures, or die three frames in. `verify.sh` runs the whole
loop and reports one line:

```bash
./verify.sh        # build + run 4s headless
./verify.sh 10     # longer window
```

It builds, checks for unresolved shared libraries, runs the binary under
`SDL_VIDEODRIVER=offscreen`, and inspects the log. It fails on: a build error,
a missing `.so`, an early exit (which for a game loop means it never started),
a segfault or abort, any `ERR` line from the engine, zero textures loaded, or
zero entities created.

A healthy game is killed by the timeout — exit code 124 is the pass condition,
not a problem.

Use `offscreen`, not `dummy`: the dummy video driver cannot satisfy
`SDL_RENDERER_ACCELERATED`, so `SDL_CreateRenderer` returns null and a perfectly
good game looks broken.
