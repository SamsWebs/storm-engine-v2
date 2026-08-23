# Storm! Engine v2 - starter game

A minimal game built against the **installed** engine. No engine source needed.

```bash
make          # build ./bin/mygame
make run      # build and play
make clean
```

`make` uses `pkg-config --cflags --libs stormengine2`, which supplies the
include path and every library the engine needs. Linking only
`-lstormenginev2` is not enough once your game calls SDL directly - the linker
refuses with `DSO missing from command line`.

## Prerequisites

The engine package pulls in the *runtime* libraries. To **compile** against it
you also need the development headers:

```bash
sudo apt install build-essential pkg-config \
                 libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
                 libsdl2-mixer-dev libglm-dev libtinyxml2-dev
```

Check the engine is visible to pkg-config:

```bash
pkg-config --modversion stormengine2
```

## What's here

```
Makefile              pkg-config based, no engine source required
src/main.cpp          creates the Game and runs it
src/game.{h,cpp}      window, renderer, and the state machine
src/states/playState  one state: an entity, three systems, a draw
assets/               your art, fonts and sounds go here
```

Run the game from this directory. Asset paths in the source are relative to the
working directory, so `make run` is the reliable way to start it.

## Two rules that will bite you otherwise

**Register systems before you create entities.** `Registry::Update()` decides
which systems an entity belongs to exactly once, when it admits the entity. A
system registered afterwards starts empty and stays empty - and adding a
component to an entity that is already live will never get it into a matching
system either. Add every component an entity needs before the `registry_.Update()`
that admits it.

**Only the active state polls events.** The SDL event queue is shared: whoever
calls `SDL_PollEvent` first consumes it and everyone else sees nothing. `Game`
deliberately does not poll.

## Adding text

Drop any `.ttf` at `assets/font.ttf` and the on-screen text appears. Fonts are
rasterised at one point size, so register one id per size:

```cpp
assetStore_->AddFont("hud-24", "./assets/font.ttf", 24);
Text::DrawCentred(renderer_, assetStore_->GetFont("hud-24"),
                  "Score: 0", windowWidth_ / 2, 24, SDL_Color{255,255,255,255});
```

`GetFont` returns `nullptr` for a missing id and `Text` is null-safe, so a
missing font draws nothing rather than crashing.

Full API walkthrough: `TUTORIAL.md` in the engine repository.
