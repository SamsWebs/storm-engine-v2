# New game scaffold — Storm! Engine v2 (any 1.x)

A complete, minimum standalone game: window, state machine, ECS registry, one
animated entity, keyboard input, quit. Copy the directory, rename, build.

Every file here exists because the engine does not ship it. `Game` and the main
loop are written by the game, not the engine, so a scaffold is the only way to
avoid re-deriving ~90 lines of boilerplate per project.

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
install** — verified against both a pre-v1.2.2 build and current 1.2.5. It
deliberately avoids the two calls that would raise the floor:

- **`Registry::DoesTagExist`** is v1.2.2+. The scaffold holds the player in a
  `std::optional<Entity>` instead of looking it up by tag each frame. Once your
  game can *kill* the player, that cache is no longer safe — ids are recycled
  and `Entity` carries no generation — so switch to a tag lookup guarded by
  `DoesTagExist` and accept the v1.2.2 floor.
- **The camera argument to `RenderSystem::Update`** is v1.2.1+, but it is a
  defaulted parameter, so *omitting* it compiles everywhere. Passing an explicit
  `nullptr` is what breaks older headers. The moment you want scrolling you pass
  `&camera` and take the v1.2.1 floor, which is almost certainly fine.

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
| `std::optional<Entity>` rather than a bare member | `Entity`'s default constructor leaves its registry pointer uninitialised, so a default-constructed member is UB to touch. |

## Verification status

All three translation units compile clean with `-Wall -std=c++17` against
**both** a pre-v1.2.2 header set and current 1.2.5 — that is the evidence for
the "any 1.x" claim above.

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
  to `RenderSystem::Update` instead of `nullptr`.
- **Collision** — `CollisionSystem` *kills* entities with a `RigidBodyComponent`
  on contact. Platformers want resolution, not death, so write a custom system.
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
