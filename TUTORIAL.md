# storm-engine-v2 Tutorial

storm-engine-v2 is a C++17 game engine built on SDL2 that uses the **Entity Component System (ECS)** pattern. This tutorial walks you through building a game from scratch using the engine's core concepts.

## Table of Contents

1. [Core Concepts](#core-concepts)
2. [Project Setup](#project-setup)
3. [The Game Loop](#the-game-loop)
4. [Game State Machine](#game-state-machine)
5. [Registry and Entities](#registry-and-entities)
6. [Built-in Components](#built-in-components)
7. [Built-in Systems](#built-in-systems)
8. [Writing a Custom Component](#writing-a-custom-component)
9. [Writing a Custom System](#writing-a-custom-system)
10. [AssetStore](#assetstore)
11. [Asset Paths and Overrides](#asset-paths-and-overrides)
12. [Text](#text)
13. [UI Scale](#ui-scale)
14. [Tilemaps and the .map Format](#tilemaps-and-the-map-format)
15. [Gamepad](#gamepad)
16. [Engine Version](#engine-version)
17. [Debug Overlay](#debug-overlay)
18. [Audio](#audio)
19. [Networking](#networking)
20. [Logger](#logger)
21. [Tags and Groups](#tags-and-groups)
22. [Putting It Together](#putting-it-together)

## Core Concepts

The engine is organized around three ideas:

- **Entity** - a unique ID representing any object in your game (player, enemy, bullet, tile).
- **Component** - plain data attached to an entity (position, velocity, sprite).
- **System** - logic that operates on every entity that has a specific set of components.

This separation keeps data and behavior independent, making it easy to add new entity types without touching existing code.

## Project Setup

Create your example directory alongside the existing ones:

```
examples/mygame/
├── Makefile              # Linux
├── Makefile.win          # Windows, optional — same sources
├── assets/
│   └── gfx/
└── src/
    ├── main.cpp
    ├── game.h
    ├── game.cpp
    └── states/
        ├── playState.h
        └── playState.cpp
```

**Makefile:**

```makefile
NAME = mygame

include ../examples.mk
```

`examples.mk` and `base.mk` handle all compiler flags, SDL2 linking, and the build rules automatically.

### Building it for Windows too

Add a second three-line file. Nothing else changes — same sources, same assets:

**Makefile.win:**

```makefile
NAME = mygame

include ../examples.win.mk
```

```bash
make -f Makefile.win deps          # once, from the repo root — slow, cross-builds SDL2
cd examples/mygame
make -f Makefile.win               # bin/win/mygame.exe
make -f Makefile.win run           # under wine
```

This cross-compiles with MinGW-w64 (`sudo apt install mingw-w64 cmake`) and
links `libstormenginev2.dll`, copying it and every DLL it needs into `bin/win/`
beside your `.exe` — Windows resolves DLLs from the executable's own directory
first and reports a missing one as a bare non-zero exit.

There is no separate "windows-mygame" example to write. `nx-platformer` and
`android-platformer` are separate trees because devkitPro and the Android NDK
need a different project layout; Windows compiles **the same sources**, which is
why the whole difference is this one file.

Two things do not port automatically. If your game polls stdin the POSIX way —
`select()` on `STDIN_FILENO`, or `fcntl(O_NONBLOCK)` — it will not compile for
Windows; MinGW has no `<sys/select.h>` and does not provide non-blocking console
handles that way. That is what keeps `netchat` and `netplay-checkers` off the
Windows list, and it is not a networking limit: `netrepl` uses the same net
module and builds fine.

## The Game Loop

Your `Game` class owns the window, renderer, and drives the main loop. It delegates all game-specific work to the `GameStateMachine`.

**game.h:**

```cpp
#pragma once
#include <SDL2/SDL.h>
#include <stormengine2/assetStore.h>
#include <stormengine2/gameStateMachine.h>
#include <stormengine2/logger.h>
#include "states/playState.h"

class Game {
public:
    void Initialize();
    void Run();
    void ProcessInput();
    void Update();
    void Render();
    void Destroy();

private:
    bool isRunning   = false;
    bool isDebugging = false;
    SDL_Window   *window   = nullptr;
    SDL_Renderer *renderer = nullptr;
    GameStateMachine gameStateMachine;
    Logger_Ptr       logger;
    AssetStore_Ptr   assetStore;
    int windowWidth = 0, windowHeight = 0;
};
```

**game.cpp:**

```cpp
#include "game.h"

Game::Game() {
    assetStore = std::make_unique<AssetStore>();
    logger     = std::make_unique<Logger>();
}

void Game::Initialize() {
    SDL_Init(SDL_INIT_EVERYTHING);

    SDL_DisplayMode dm;
    SDL_GetCurrentDisplayMode(0, &dm);
    windowWidth = dm.w; windowHeight = dm.h;

    window   = SDL_CreateWindow(NULL, SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, windowWidth,
                                windowHeight, SDL_WINDOW_BORDERLESS);
    renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

    // Hand off to the first game state. Pass isRunning by reference so
    // the state can signal when it's time to quit.
    gameStateMachine.changeState(
        new PlayState(renderer, windowWidth, windowHeight,
                      isDebugging, std::move(assetStore), isRunning));

    isRunning = true;
}

void Game::Run() {
    Initialize();
    while (isRunning) {
        ProcessInput();
        Update();
        Render();
    }
}

// Let the active state own all event polling - do NOT call SDL_PollEvent
// here, or you will consume events before the state can see them.
void Game::ProcessInput() { gameStateMachine.processInput(); }
void Game::Update()       { gameStateMachine.update();       }
void Game::Render()       { gameStateMachine.render();       }

void Game::Destroy() {
    gameStateMachine.clean();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
```

**main.cpp:**

```cpp
#include "game.h"
int main(int argc, char *argv[]) {
    Game game;
    game.Run();
    game.Destroy();
    return 0;
}
```

> **Important:** Never call `SDL_PollEvent` in both `Game::ProcessInput` and `PlayState::processInput`. The event queue is shared - whoever polls first consumes the events and the other sees nothing. Let the active state own all event polling.

## Game State Machine

The `GameStateMachine` manages a stack of `GameState` objects. You can push, pop, or replace states.

```cpp
gameStateMachine.changeState(new PlayState(...));  // replace current state
gameStateMachine.pushState(new PauseState(...));   // push on top (old state pauses)
gameStateMachine.popState();                       // return to previous state
```

Each state implements these methods:

```cpp
class MyState : public GameState {
public:
    void processInput() override;
    void update()       override;
    void render()       override;
    bool onEnter()      override;  // called once when state becomes active
    bool onExit()       override;  // called once when state is leaving
    std::string getStateID() const override { return "MY_STATE"; }
};
```

`GameState` already includes all engine headers (`ecs.h`, `assetStore.h`, `systems/render.h`, etc.), so you don't need to repeat those includes in your state headers.

That convenience costs 146,748 preprocessed lines per file. If your game does not use most of the engine - no ECS, no built-in systems - include
`<stormengine2/states/gameStateBase.h>` instead. It is the same `GameState`
interface with none of the extras (80,265 lines, a 45% saving), and you include
what you actually use. `gameState.h` includes it, so the two never drift and you
can switch either way at any time.

## Registry and Entities

The `Registry` is the heart of the ECS. It creates entities, attaches components, and connects entities to systems.

```cpp
Registry registry;

// Create an entity
Entity player = registry.CreateEntity();

// Attach components
player.AddComponent<TransformComponent>(glm::vec2(100, 200), glm::vec2(1, 1), 0.0);
player.AddComponent<RigidBodyComponent>(glm::vec2(0, 0));
player.AddComponent<SpriteComponent>("player-sprite", 64, 64, 1);

// Read or modify a component
auto &transform = player.GetComponent<TransformComponent>();
transform.position.x += 10;

// Check for a component
if (player.HasComponent<SpriteComponent>()) { ... }

// Remove a component
player.RemoveComponent<RigidBodyComponent>();

// Destroy the entity (deferred until registry.Update())
player.Kill();
```

> Entity creation and destruction are **deferred** - they take effect the next time you call `registry.Update()`. Always call `registry.Update()` at the start of your `update()` method before running systems.

```cpp
void PlayState::update() {
    // Sleeps out the rest of the 60 FPS budget and hands back how long the
    // frame took. The default clamps a hitch to 50ms so nothing teleports;
    // pass 0 if you want the raw delta.
    const double deltaTime = CapFrameRate();

    registry.Update(); // flush pending entity adds/kills first
    registry.GetSystem<MovementSystem>().Update(deltaTime);
    registry.GetSystem<AnimationSystem>().Update();
    registry.GetSystem<ContactSystem>().Update();
}
```

## Built-in Components

All built-in components are in `<stormengine2/components/>`.

### TransformComponent
Position, scale, and rotation of an entity.

```cpp
#include <stormengine2/components/transform.h>

// TransformComponent(position, scale, rotationDegrees)
entity.AddComponent<TransformComponent>(
    glm::vec2(x, y),      // screen position
    glm::vec2(1.5, 1.5),  // scale
    0.0                    // rotation in degrees
);
```

### RigidBodyComponent
Velocity used by `MovementSystem` to move the entity each frame.

```cpp
#include <stormengine2/components/rigidBody.h>

entity.AddComponent<RigidBodyComponent>(glm::vec2(200.0, 0.0)); // px/sec rightward
```

### SpriteComponent
Links the entity to a texture in the `AssetStore` and defines the source rectangle for sprite sheets.

```cpp
#include <stormengine2/components/sprite.h>

// SpriteComponent(assetId, frameWidth, frameHeight, zIndex)
entity.AddComponent<SpriteComponent>("player", 64, 64, 2);

// Flip the sprite horizontally (useful for mirroring characters)
entity.GetComponent<SpriteComponent>().flip = SDL_FLIP_HORIZONTAL;
```

`zIndex` controls draw order - higher values render on top.

### AnimationComponent
Animates a horizontal sprite sheet by cycling `srcRect.x` each frame.

```cpp
#include <stormengine2/components/animation.h>

// AnimationComponent(numFrames, frameSpeedRate, vertical, isLooped)
entity.AddComponent<AnimationComponent>(
    5,     // number of frames in the strip
    12,    // frames per second
    false, // false = horizontal sheet, true = vertical sheet
    true   // loop the animation
);
```

The sprite sheet must be laid out horizontally with each frame exactly `frameWidth` pixels wide.

### BoxColliderComponent
An axis-aligned bounding box, read by `ContactSystem`.

```cpp
#include <stormengine2/components/boxCollider.h>

entity.AddComponent<BoxColliderComponent>(64, 64); // width, height
```

### CircleColliderComponent
A round collider, also read by `ContactSystem`. Give an entity one *or* the
other — a body carrying both is a bug, and the box wins.

```cpp
#include <stormengine2/components/circleCollider.h>

entity.AddComponent<CircleColliderComponent>(16.0f, glm::vec2(16, 16));
//                                           radius, offset to the CENTRE
```

Two differences from `BoxColliderComponent`, both deliberate. The offset places
the **centre**, not a corner, so centring a circle on a 32x32 sprite drawn from
`transform.position` wants `offset = {16, 16}` rather than `{0, 0}`. And the
radius is a `float`, because a radius is usually half a sprite cell and rounding
3.5 px to 3 or 4 is visible on a small body.

Use a circle for anything round: a puck riding the boards, characters pushed
apart by a separation radius, a shot glancing off a post. Against a box corner a
circle reports a diagonal normal, where a box snaps that to an axis — the
difference is how the game feels, not just how it computes.

`transform.scale` scales the radius by its **larger absolute axis**. A circle
cannot be an ellipse, so a non-uniform scale has no right answer; the larger axis
at least never leaves a body quietly smaller than the sprite it stands for, and
the absolute value keeps a mirrored sprite (`scale.x = -1`) from inverting its
collider. The offset, like a box's, is world pixels and is *not* scaled.

## Built-in Systems

All built-in systems are in `<stormengine2/systems/>`. Register them with `registry.AddSystem<T>()` before creating any entities that need them.

```cpp
registry.AddSystem<MovementSystem>();
registry.AddSystem<RenderSystem>();
registry.AddSystem<AnimationSystem>();
registry.AddSystem<ContactSystem>();
registry.AddSystem<RenderColliderSystem>(); // debug: draws collider outlines
```

| System | Requires | What it does |
|---|---|---|
| `MovementSystem` | Transform + RigidBody | Moves entities by `velocity * deltaTime` each frame |
| `RenderSystem` | Transform + Sprite | Draws all sprites sorted by `zIndex` |
| `AnimationSystem` | Sprite + Animation | Advances the sprite sheet frame |
| `ContactSystem` | Transform (plus a box **or** circle collider) | Reports overlaps with a normal and depth, plus begin/end callbacks. Boxes and circles pair against each other. Never kills or moves anything |
| `RenderColliderSystem` | Transform (plus a box **or** circle collider) | Draws collider outlines (debug): rectangles for boxes, traced circles for circle colliders. Takes an optional camera, like `RenderSystem` |

`ContactSystem` reports; it never acts. Read `GetContacts()` and decide what a
contact means, or install `SetOnBeginContact` / `SetOnEndContact` for the
once-per-pair transitions:

```cpp
registry_.AddSystem<ContactSystem>();
auto &contacts = registry_.GetSystem<ContactSystem>();

// Skip pairs you never care about - this is where layers and sensors live.
contacts.SetPairFilter([](const Entity &a, const Entity &b) {
  return !(a.BelongsToGroup("bullets") && b.BelongsToGroup("bullets"));
});

contacts.SetOnBeginContact([](const Contact &c) {
  // Fires once when the pair starts touching, not every frame.
});

// ... then in update(), after registry_.Update():
contacts.Update();
for (const auto &c : contacts.GetContacts()) {
  // c.a always holds the lower entity id, so c.normal points a -> b.
  auto &transform = c.a.GetComponent<TransformComponent>();
  transform.position -= ContactSystem::MinimumTranslation(c);
}
```

Three things to know.

A shared edge is *not* a contact - the overlap test is strict, unlike an
inclusive test that would count a shared edge as a contact.

System membership is computed once, when `Registry::Update()` admits an
entity - but `ContactSystem` and `RenderColliderSystem` are not bound by it,
because they require `TransformComponent` alone and re-read the collider every
frame. A live entity that already had a transform when it was admitted can gain
or lose a box or circle collider and both will pick the change up on the next
`Update()`. An entity admitted with *no* transform is still not a member, and
adding one later will not make it one.

**Do not build tilemap collision out of one collider entity per tile.** The
manifold picks the axis of least penetration *per box*, so a character sliding
along a floor made of adjacent tile colliders can pick up a sideways normal
from a tile it barely overlaps and get shoved out of the wall - the classic
ghost-vertex problem, and the reason Box2D has a dedicated chain shape for it.
`ContactSystem` suits distinct bodies rather than terrain: a puck and the
boards, bullets and enemies, a player and a handful of triggers. The broadphase
is a uniform grid, so a column of bodies costs no more than a row, and the
practical ceiling is the manifold work rather than the pairing. For tiles,
snap to the grid instead the way `examples/platformer` does - it reads
`IsSolid(col, row)` and resolves one axis at a time against tile boundaries
(`src/states/playState.cpp:230-245`), which cannot catch on a seam because it
never computes a normal at all.

**Calling systems in your update/render:**

```cpp
void PlayState::update() {
    registry.Update();
    registry.GetSystem<MovementSystem>().Update(deltaTime);
    registry.GetSystem<AnimationSystem>().Update();
    registry.GetSystem<ContactSystem>().Update();
}

void PlayState::render() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    registry.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);

    if (isDebugging_)
        registry.GetSystem<RenderColliderSystem>().Update(renderer_);

    // Present(), NOT SDL_RenderPresent(). See below.
    Present(renderer_, overlays_);
}
```

**Call `Present(renderer_, overlays_)`, not `SDL_RenderPresent`.** `Present` is
a non-virtual member of `GameState` that draws the overlay list and then
presents, and it is the only `SDL_RenderPresent` in the engine. That sounds
like tidiness; the reason is ordering. Anything you draw after
`SDL_RenderPresent` is drawn into a back buffer nobody will ever look at, so a
debug overlay or a debug font registered after the present is a diagnostic
that looks configured and never appears.

`overlays_` is an `OverlayList` member of your state - the list belongs to the
game, and `Present` takes it by reference, so adding an overlay changes
neither `GameState`'s layout nor its vtable:

```cpp
// In the state's constructor, or once on enter.
overlays_.Add("debug", [this](SDL_Renderer *r) {
    debug_.Draw(r, assetStore_->GetFont("hud-18"), 8, 8);
});
overlays_.Add("prompt", [this](SDL_Renderer *r) { drawPrompt(r); });

// Later, if the overlay is no longer relevant.
overlays_.Remove("prompt");
```

Re-adding a name **replaces the callback in place and keeps its position**, so
an overlay that re-registers every frame does not climb to the top of the stack
every frame. `Add` returns false for an empty name or a null callback and
changes nothing.

## Writing a Custom Component

A component is a plain struct with a default constructor and any constructors you need. No base class is required - the engine uses templates to identify component types at compile time.

```cpp
// src/components/healthComponent.h
#pragma once

struct HealthComponent {
    int maxHp = 100;
    int hp    = 100;

    HealthComponent() = default;
    HealthComponent(int max) : maxHp{max}, hp{max} {}
};
```

Attach it like any built-in component:

```cpp
enemy.AddComponent<HealthComponent>(50);
auto &health = enemy.GetComponent<HealthComponent>();
health.hp -= 10;
```

## Writing a Custom System

Extend `System`, declare which components entities must have with `RequireComponent<T>()` in the constructor, then iterate `GetSystemEntities()` in your update method.

```cpp
// src/systems/healthSystem.h
#pragma once
#include <stormengine2/ecs.h>
#include "../components/healthComponent.h"

class HealthSystem : public System {
public:
    HealthSystem() {
        RequireComponent<HealthComponent>();
    }

    void Update() {
        for (auto &entity : GetSystemEntities()) {
            auto &health = entity.GetComponent<HealthComponent>();
            if (health.hp <= 0) {
                entity.Kill();
            }
        }
    }
};
```

Register and call it like any built-in system:

```cpp
registry.AddSystem<HealthSystem>();

// in update():
registry.GetSystem<HealthSystem>().Update();
```

A system only sees entities that have **all** of its required components. If you call `RequireComponent<HealthComponent>()` and `RequireComponent<TransformComponent>()`, the system ignores any entity missing either one.

## AssetStore

`AssetStore` loads and caches textures, fonts and sounds by string ID. Paths are relative to where the binary runs.

```cpp
assetStore->AddTexture(renderer, "player", "./assets/gfx/player.png");
assetStore->AddTexture(renderer, "enemy",  "./assets/gfx/enemy.png");

// A TTF_Font is rasterised at ONE point size, so register one id per size
// you draw at. Requires TTF_Init() to have been called.
assetStore->AddFont("hud-18",   "./assets/fonts/font.ttf", 18);
assetStore->AddFont("title-32", "./assets/fonts/font.ttf", 32);

// Requires Mix_OpenAudio() to have been called.
assetStore->AddSound("jump", "./assets/sfx/jump.wav");

SDL_Texture *tex   = assetStore->GetTexture("player");
TTF_Font    *font  = assetStore->GetFont("hud-18");
Mix_Chunk   *sfx   = assetStore->GetSound("jump");

Mix_PlayChannel(-1, sfx, 0);

// Frees textures, fonts AND sounds. Called automatically in the destructor.
assetStore->ClearAssets();
```

Every getter returns `nullptr` for a missing ID rather than throwing, so
null-check the result - nothing aborts under the Switch build's
`-fno-exceptions`.

**Call `ClearAssets()` before `TTF_Quit()`, `Mix_CloseAudio()` or `SDL_Quit()`.**
Those calls free every open font and chunk themselves, so a store destroyed
afterwards hands already-freed pointers to `TTF_CloseFont`. The store usually
outlives the state that shut the subsystems down, which is exactly the order
that goes wrong.

The store does not initialise SDL_ttf or SDL_mixer. Without `TTF_Init()` or
`Mix_OpenAudio()` you get a logged failure and a `nullptr`, not a crash.

The `AssetStore_Ptr` (`std::unique_ptr<AssetStore>`) is created in `Game` and moved into the first state via `std::move`. If you need it in subsequent states, pass a raw pointer or reference rather than moving ownership again.

### Asset packs

Call `LoadPack` once at startup and every path-based `Add*` afterwards loads
from the pack when the pack holds that entry, falling back to the loose file
when it does not:

```cpp
assetStore->LoadPack("assets.pak");   // false just means "no pack" - not fatal
```

A development tree with a partial pack still works and **no call site
changes**. A missing or corrupt pack is logged and leaves the loose-file
contract standing, so a broken pack is a performance problem, not a
correctness one.

### Raw bytes, and surfaces you built yourself

`ReadBlob` gets you the bytes for anything the store does not cache - a
pixel-processing pipeline, a window icon, a font you open yourself. It uses
the same pack-then-file rule:

```cpp
std::vector<uint8_t> bytes;
if (assetStore->ReadBlob("assets/gfx/logo.png", &bytes)) {
    // bytes came from the pack, or from the file
}
```

The bytes are a **copy**, so they outlive the store. Whether they must outlive
the *call* depends on the loader, and this is the part worth knowing:

| you build | decodes | the bytes must live until |
|---|---|---|
| a texture (`IMG_Load_RW`) | synchronously | the call returns |
| a sound (`Mix_LoadWAV_RW`) | synchronously | the call returns |
| **a font (`TTF_OpenFontRW`)** | **lazily, at render time** | **you close the font** |

SDL_ttf keeps the `SDL_RWops` and reads glyphs out of it when it *draws*. Let
the vector die and you get garbage glyphs or a crash minutes later, with
nothing pointing back at the load. So if you open your own font over a blob,
keep that vector alive as long as the font is open.

If you have already built an `SDL_Surface` yourself - recoloured it, or decoded
it by hand - `AddTextureFromSurface` takes it. **It frees the surface**:
`SDL_CreateTextureFromSurface` copies, so the surface is dead the moment the
call returns.

The whole path convention, including overrides, is in
[`docs/assets.md`](docs/assets.md).


## Asset Paths and Overrides

`<stormengine2/assetPath.h>` is the seam between the path a game writes and the
file that gets opened. Three shapes, one rule:

```
"assets/gfx/player.png"       what a game writes
         |  AssetPath()
         v
"gfx/player.png"              the pack entry name == the logical asset path
         +-- AssetFilePath(.., "assets")       -> "assets/gfx/player.png"
         +-- AssetOverridePath(.., "userdata") -> "userdata/gfx/player.png"
```

There is **one** asset path, not several that happen to agree: it is relative
to the assets root, and it is simultaneously the game's logical path and the
pack's entry name, because a pack's entries *are* relative to that root.

`AssetPath` normalises three things - backslashes to forward slashes (a pack
built on Windows must load on Linux), a leading `./`, and a leading `assets/`
- and it is idempotent, so calling it twice is safe.

`ResolveAssetFile` is the one-call version of "shipped content is read-only,
the player's replacement lives beside the save":

```cpp
#include <stormengine2/assetPath.h>

std::string which;
const std::string file = ResolveAssetFile(
    "assets/gfx/player.png",  // what the game normally writes
    "assets",                 // where shipped content is read from
    "userdata/mods",          // where replacements are read from
    &which);                  // "override" or "shipped"
```

It creates and writes nothing. An empty `writableBase` means "no override
support" and gets the shipped file. **Keep the `which` out-parameter** if you
can: a loader that silently prefers a replacement is a support ticket nobody
can answer, because the game shows the wrong art and nothing records that it
looked.

Details, including the pack-versus-loose fallback and the blob lifetime table:
[`docs/assets.md`](docs/assets.md).

## Text

Drawing one line with SDL_ttf is a five-call dance with a failure path at every
step. `Text` (`<stormengine2/text.h>`) is that dance, done once:

```cpp
#include <stormengine2/text.h>

TTF_Font *font = assetStore->GetFont("hud-18");

// Top-left at (10, 10). Returns the size drawn; {0, 0} means nothing was.
Text::Draw(renderer, font, "Score: 400", 10, 10, {255, 255, 255, 255});

// Horizontally centred - no more hand-guessed "windowWidth / 2 - 30" offsets,
// which drift the moment the string or the point size changes.
Text::DrawCentred(renderer, font, "GAME OVER", windowWidth / 2, 200,
                  {255, 210, 50, 255});

// Measure without drawing, for your own layout.
SDL_Point size = Text::Measure(font, "Score: 400");
```

Header-only and null-safe: a null renderer or font draws nothing and returns
`{0, 0}`, which is exactly what `GetFont` hands you for an unregistered ID. It
never opens or closes a font, and never leaks the intermediate surface or
texture.

```cpp
// Right-aligned against a fixed edge - the HUD number that must not shift
// left as the score grows from 3 to 4 digits.
Text::DrawRight(renderer, font, std::to_string(score), windowWidth - 10, 10,
                {255, 255, 255, 255});
```

### Text that has to fit

A string that might be too long is a layout problem, and the usual fix -
measure it yourself and then decide what to cut - puts the truncation logic in
every state. `FitText` and `DrawFitted` do the cutting:

```cpp
// Measure only, so you can lay out around it. `truncated` tells you whether
// anything was actually cut, which is the difference between "Score: 400" and
// "Score: 4000" both fitting and one of them being silently mangled.
FittedText fitted = Text::FitText(font, message, 200);

// Or fit and draw in one call.
Text::DrawFitted(renderer, font, message, 200, x, y, color);
```

`FittedText` is `{ size, text, truncated }`, and `text` is what was (or would
be) drawn - which may be shorter than what you passed in. The mark is a UTF-8
ellipsis by default and you can pass your own.

For a multi-part footer - control hints, "press X to continue" - `FitFooter`
lays parts out over a line budget and `DrawFooter` draws the result:

```cpp
// Wraps as many parts as fit maxLines, two spaces between them.
FooterLayout layout = Text::FitFooter(font, {"Move: WASD", "Jump: Space",
                                            "Quit: Esc"}, 300, 2);
if (layout.dropped) {
    // Something did not fit. `overflow` is the worse case: a part wider than
    // maxWidth on its own, which no amount of wrapping can save.
}
Text::DrawFooter(renderer, font, hints, 300, x, y, 18, color);
```

`layout.width` is the **widest** line, so that is the number to align
something else against.

## UI Scale

Every hard-coded pixel in a layout is wrong on exactly one window size.
`<stormengine2/ui/scale.h>` gives you one factor instead:

```cpp
#include <stormengine2/ui/scale.h>

// Built once, when you know the window height.
UiScale ui(windowHeight);

SDL_Rect button{ui.Px(16), ui.Px(24), ui.Px(180), ui.Px(36)};
Text::Draw(renderer, fontAt(ui.FontPt(16)), "Start", button.x, button.y, white);
```

`Px` scales a pixel measurement and `FontPt` a point size, both against a 720px
reference, so the same numbers give the same proportions at any height. Round
values never collapse to zero, negatives keep their sign, and a window height
of zero or less is clamped rather than dividing by it.

For one-off values with no object to keep, the free functions take the height:

```cpp
int margin = Px(16, windowHeight);
```

Header-only, pure, and SDL-free - no `.cpp` to add to any build list.

## Tilemaps and the .map Format

`TileMapLoader` reads the space-separated format the tile editor writes, and
also the older comma-separated index format. The editor format needs no
tileset PNG, because each record carries its own source rectangle:

```cpp
// No PNG argument, and a tile size - the loader derives grid positions by
// dividing world coordinates by it.
TileMapLoader loader("assets/tilemaps/level.map", "", 16);
const Map &tiles = loader.getMap();
```

**A `.map` can carry a version header**, `<stormengine2/tilemapFormat.h>`:

```
storm-map 1
tiles grass 8 8 0 0 0 0 0 1 1 0 0
```

A file with **no** header reads as version 1 and loads unchanged, so every map
written before the header existed keeps working. A file declaring a version
this build does not know is **refused** with a diagnostic naming the number,
rather than half-read - half-reading produces a level that renders and is
wrong, which is worse than one that does not load.

If you write `.map` files yourself, use `TileMapVersionLine()` and
`ReadTileMapVersion` rather than hand-writing the header, so the version you
stamp is the version the reader enforces. See [`docs/ROADMAP.md`](docs/ROADMAP.md)
§2.5.1 for why the record parser is still hand-rolled twice.

## Engine Version

`<stormengine2/version.h>` is the compile-time version, generated from
`Makefile.debian` so it cannot drift from what was built:

```cpp
#include <stormengine2/version.h>

SDL_Log("running %s", storm::kEngineVersionString);   // "v2.3.1"
SDL_Log("major %d", kEngineVersionMajor);

// For a save-file or asset-format check. Exact string compare, no parsing.
if (!VersionEquals("2.3.1")) { /* refuse */ }
```

There is no runtime "which engine am I" call beyond `VersionString()`; the
constant is the answer, and it is `constexpr`.

## Debug Overlay

`<stormengine2/debugOverlay.h>` gives you fps, the slowest system, and a ring
of recent errors. It is opt-in, hidden until toggled, and **takes its clock
from you** rather than calling `SDL_GetTicks`, so it works in a test and does
not make your frame time depend on the overlay:

```cpp
DebugOverlay debug_;   // a member of your PlayState

void PlayState::update(double dt) {
    debug_.BeginFrame(dt);
    debug_.BeginSystem("movement");
    movement_.Update(dt);
    debug_.EndSystem(ms);          // whatever you measured
    debug_.EndFrame();
    debug_.SetEntityCount(registry_.GetEntityCount());
    debug_.Error("could not spawn player at %d", tileId);
}

void PlayState::render(SDL_Renderer *renderer, const AssetStore &assets) {
    // Registered in the OverlayList, so it draws BEFORE the present. An
    // overlay drawn after SDL_RenderPresent is one nobody ever sees.
    overlays_.Add("debug", [&](SDL_Renderer *r) {
        debug_.Draw(r, assets.GetFont("hud-18"), 8, 8);
    });
    Present(renderer, overlays_);
}
```

`Toggle()` flips visibility and is usually bound to a key. `DebugStats` on its
own is the pure part - no drawing - if you want the numbers without the
overlay.

## Audio

`AssetStore` decodes and caches sounds, and stops there. Everything after
`GetSound` was the game's problem, and the usual shape of that problem is
`Mix_PlayChannel(-1, chunk, 0)`. The `-1` means "first free channel", so a
sound lands wherever it happens to fit, and there is no volume model anywhere
above SDL_mixer. `SoundMixer` (`<stormengine2/audio/soundMixer.h>`) is that
layer, and the policy it decides with (`<stormengine2/audio/mixer.h>`) is pure:
no audio device, so it is the part you can test.

```cpp
#include <stormengine2/audio/soundMixer.h>

class PlayState : public GameState {
  SoundMixer mixer_;   // opens nothing on construction
  AssetStore_Ptr assetStore_;
};
```

**Open it, and keep going if it fails.** `Open()` returns false and logs when
there is no device, and every other call then stays safe and silent - a game
that runs fine without sound should not have to branch on it.

```cpp
void PlayState::onEnter() {
  assetStore_ = std::make_unique<AssetStore>();
  mixer_.Open();

  assetStore_->AddSound("move", "assets/sfx/move.wav");
  assetStore_->AddSound("win", "assets/sfx/win.wav");
}
```

**Give sounds a priority, because they are not equal.** This is the part
`Mix_PlayChannel(-1, ...)` cannot express. A move fires on every ply; a win is
the payoff. Unranked, a burst of moves can hold every channel and the win is
the sound you never hear.

```cpp
// A win outranks a capture, which outranks a move. A sound that cannot get a
// channel better than what is already playing is dropped rather than
// interrupting it.
mixer_.Play(assetStore_.get(), "win",     kHighestPriority);
mixer_.Play(assetStore_.get(), "capture", kNormalPriority + 10);
mixer_.Play(assetStore_.get(), "move",    kLowestPriority);
```

`Play` is pack-aware for free: the store is what knows whether the bytes came
from a pack or a loose file, and `Play` only asks it for the chunk.

**Volume, and one number to mute everything.** Three buses - master, music,
sfx - each `0..1`, with the master multiplying both.

```cpp
mixer_.Volumes().SetMaster(0.5f);
mixer_.Volumes().SetSfx(0.8f);
mixer_.ApplySfxVolume();   // Play() does this for you; a settings screen
                           // wants it after changing the model.
```

Values are clamped on the way in, and a NaN becomes silence rather than
poisoning both buses - it would otherwise reach `Mix_Volume`'s int conversion
with no diagnostic anywhere.

**The engine does not own music.** There is no `PlayMusic`, and that is a
decision rather than a gap: SDL_mixer has one music channel, and a game with a
score or dynamic music needs to hold that handle itself. What the mixer gives
you is the *volume*, so a game calling `Mix_PlayMusic` itself still obeys the
same settings screen:

```cpp
mixer_.Volumes().SetMusic(0.4f);
mixer_.ApplyMusicVolume();
Mix_PlayMusic(music_);   // yours; the mixer's only part is the volume
```

**Ordering, because the wrong way is a double free.** `Close()` closes the
device and `Mix_CloseAudio` frees every open chunk, and the `AssetStore` owns
those chunks. Clear the store first:

```cpp
void PlayState::onExit() {
  assetStore_->ClearAssets();   // frees the Mix_Chunk*s
  mixer_.Close();               // closes the device
}
```

`AssetStore` is an engine member, so `~SoundMixer` also calls `Close()`; the
explicit call is about ordering against the store, not about tidiness.

## Networking

The net layer (`<stormengine2/net/net.h>`) is hand-rolled non-blocking UDP:
`NetServer`, `NetClient`, and the snapshot types. It is **not** covered by this
tutorial - see [`docs/networking.md`](docs/networking.md), which documents the
handshake, the windowing rules and the object sizes.

One helper is worth knowing here because it is about hosting, not protocol.
`<stormengine2/net/hostAddress.h>` tells the host its own LAN address:

```cpp
#include <stormengine2/net/hostAddress.h>

// Never loopback: a lobby advertising 127.0.0.1 only works on the host.
NetCandidate lan = BestLocalAddress();
if (lan.address.empty()) {
    // No usable address - say so in the lobby rather than advertising one
    // nobody can reach.
}
```

It **ranks** the answers rather than returning the first non-loopback one,
because every machine has several and they are not equally useful: a developer
laptop carries 127.0.0.1, a docker bridge, a VPN tunnel, and exactly one
interface a player on the same network can reach. `Ranked()` gives you the
whole list for a lobby UI; `ChooseBest()` gives the default.

On Switch it returns nothing: there is no `getifaddrs` there, and a stale guess
is worse than an honest "no address found".

## Gamepad

`Gamepad` (`<stormengine2/input/gamepad.h>`) wraps one physical controller.
Hold it by value in your `Game` and pass a pointer to the states that need it.

> Not to be confused with `input/virtualGamepad.h`, which is an on-screen
> *touch* pad for mobile and is SDL-free.

```cpp
#include <stormengine2/input/gamepad.h>

Gamepad pad;                 // in Game, by value
pad.OpenFirstAttached();     // SDL does not always send ADDED for a pad that
                             // was already plugged in, so ask directly

// In processInput, feed it device add/remove events:
while (SDL_PollEvent(&event)) {
  pad.HandleEvent(event);
}

// Then sample it once, after the event loop:
pad.Update();

// Held this frame:
if (pad.Down(GamepadButton::Right)) { ... }

// Only on the frame the button went down - menus need this, or holding the
// button retriggers every frame:
if (pad.Pressed(GamepadButton::A))    { Shoot(); }
if (pad.Released(GamepadButton::A))   { ... }

// Analog. Sticks are -1..1 with the deadzone already removed and rescaled, so
// a light lean really does give a small number. Triggers are 0..1.
const float x = pad.Current().leftX;
const float speed = pad.Current().triggerRight;
```

`Down()` is true for the d-pad **or** the left stick past half travel, so a
game binds a direction once and either input drives it. Read `Current().leftX`
directly when you want the analog value rather than the digital one.

**Call `Shutdown()` before `SDL_Quit()` or `SDL_QuitSubSystem()`.**
`SDL_GameControllerQuit` force-closes and frees every open controller, so a
`Gamepad` destroyed afterwards calls `SDL_GameControllerClose` on freed memory.
The destructor is then a harmless second call.

`SetDeadzone()` overrides the default (8000, about 24% of the axis range) if
that feels wrong for your game.

## Lighting

`<stormengine2/lighting.h>` gives a scene a lit pool and a dark surround without
shaders, so it works on every target the engine builds for.

```cpp
#include <stormengine2/lighting.h>

storm::LightingOverlay lighting;

void PlayState::onEnter() {
    storm::LightingOverlay::Params params;
    params.width  = 1280;
    params.height = 720;
    params.centre = glm::vec2(640, 360);   // omit to centre on the screen
    params.radius = 300.0f;                // omit for half the smaller side
    params.keyOpacity = 72;                // the knob to reach for first
    lighting.Build(renderer_, params);
}

void PlayState::render() {
    registry.GetSystem<RenderSystem>().Update(renderer_, *assetStore_, &camera);
    lighting.Draw(renderer_);   // LAST, over the finished frame
    SDL_RenderPresent(renderer_);
}
```

Two layers are built once and cached at quarter resolution: a warm key layer
whose alpha follows a radial falloff, and a cool vignette whose alpha is the
*inverse* of that same falloff. `Draw` is two `SDL_RenderCopy` calls no matter
the screen size.

The pairing is the technique. One warm layer is a colour filter; the cool layer
where the warm one is absent gives the frame two colour temperatures with a
boundary between them, and the eye reads that boundary as light.

Draw it **last**. It is a post-pass over the finished frame, so anything drawn
after it is unlit — right for a HUD, wrong for the world. `Build` returns `false`
on a null renderer or a non-positive size and leaves the overlay unbuilt, and
`Draw` on an unbuilt overlay is a no-op, so a failure costs you the lighting
rather than the frame. Call `Build` again to change resolution; it releases the
previous pair first.

## Logger

```cpp
Logger logger;
logger.Log("Player spawned at position (100, 200)");
logger.Err("Failed to load texture: player.png");
```

Output is printed to stdout with a timestamp and color coding (green for info, red for errors). All entries are also stored in `Logger::messages` if you want to display them in-game.

## Tags and Groups

Tags and groups let you quickly look up specific entities without iterating everything.

**Tags** - one unique tag per entity, one entity per tag:

```cpp
player.Tag("player");

// Retrieve by tag (throws if tag doesn't exist)
Entity p = registry.GetEntityByTag("player");

// Safe check before retrieving
if (registry.EntityHasTag(p, "player")) { ... }
```

**Groups** - multiple entities can share a group name:

```cpp
bullet.Group("bullets");
enemy.Group("enemies");

// Iterate a group safely
if (registry.DoesGroupExist("enemies")) {
    for (auto &e : registry.GetEntitiesByGroup("enemies")) {
        // process each enemy
    }
}
```

## Putting It Together

A minimal `PlayState` that creates an animated, moving player sprite:

```cpp
// states/playState.cpp
#include "playState.h"

const std::string PlayState::s_playID = "PLAY";

PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore_Ptr assetStore, bool &isRunning)
    : renderer_{renderer}, windowWidth_{windowWidth}, windowHeight_{windowHeight},
      isDebugging_{isDebugging}, assetStore_{std::move(assetStore)},
      isRunning_{isRunning} {

    registry_.AddSystem<MovementSystem>();
    registry_.AddSystem<RenderSystem>();
    registry_.AddSystem<AnimationSystem>();
    registry_.AddSystem<ContactSystem>();
    registry_.AddSystem<RenderColliderSystem>();

    assetStore_->AddTexture(renderer_, "player", "./assets/gfx/player.png");

    Entity player = registry_.CreateEntity();
    player.Tag("player");
    player.AddComponent<TransformComponent>(glm::vec2(100, 300), glm::vec2(1, 1), 0.0);
    player.AddComponent<RigidBodyComponent>(glm::vec2(0, 0));
    player.AddComponent<SpriteComponent>("player", 64, 64, 1);
    player.AddComponent<AnimationComponent>(4, 10, false, true);
    player.AddComponent<BoxColliderComponent>(64, 64);
}

void PlayState::processInput() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)                                { isRunning_ = false; return; }
        if (e.type == SDL_KEYDOWN &&
            e.key.keysym.sym == SDLK_ESCAPE)                  { isRunning_ = false; return; }
    }

    auto &vel = registry_.GetEntityByTag("player").GetComponent<RigidBodyComponent>();
    vel.velocity = glm::vec2(0, 0);
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if (keys[SDL_SCANCODE_LEFT])  vel.velocity.x = -200;
    if (keys[SDL_SCANCODE_RIGHT]) vel.velocity.x =  200;
    if (keys[SDL_SCANCODE_UP])    vel.velocity.y = -200;
    if (keys[SDL_SCANCODE_DOWN])  vel.velocity.y =  200;
}

void PlayState::update() {
    const double deltaTime = CapFrameRate();

    registry_.Update();
    registry_.GetSystem<MovementSystem>().Update(deltaTime);
    registry_.GetSystem<AnimationSystem>().Update();
    registry_.GetSystem<ContactSystem>().Update();
}

void PlayState::render() {
    SDL_SetRenderDrawColor(renderer_, 30, 30, 30, 255);
    SDL_RenderClear(renderer_);
    registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);
    if (isDebugging_)
        registry_.GetSystem<RenderColliderSystem>().Update(renderer_);
    SDL_RenderPresent(renderer_);
}

bool PlayState::onEnter() { m_loadingComplete = true; return true; }
bool PlayState::onExit()  { m_exiting = true;         return true; }
```

## Reference: What Lives Where

| Thing | Location |
|---|---|
| Engine headers | `/usr/local/include/stormengine2/` (Linux, installed) |
| Example source | `examples/<name>/src/` |
| Assets | `examples/<name>/assets/` - paths are relative to the binary |
| Binary output | `examples/<name>/bin/` (Linux), `examples/<name>/bin/win/` (Windows, with its DLLs) |
| Build rules | `base.mk`, `examples/examples.mk`; `examples/examples.win.mk` for Windows |

The headers you will actually include, and what each is for:

| Header | For |
|---|---|
| `<stormengine2/ecs.h>` | `Registry`, `Entity`, the systems |
| `<stormengine2/components/…>` | `TransformComponent`, `SpriteComponent`, `AnimationComponent`, colliders |
| `<stormengine2/systems/…>` | `RenderSystem`, `MovementSystem`, `ContactSystem`, the debug collider overlay |
| `<stormengine2/states/gameState.h>` | `GameState`, `GameStateMachine`, `OverlayList`, `Present` |
| `<stormengine2/assetStore.h>` | `AssetStore` - the `Add*`/`Get*` path above |
| `<stormengine2/assetPath.h>` | the asset path convention and the override resolver |
| `<stormengine2/text.h>` | `Text` - one call instead of the SDL_ttf dance |
| `<stormengine2/ui/scale.h>` | one scale factor for a whole layout |
| `<stormengine2/tilemapLoader.h>` | `TileMapLoader`, `Tile`, `Map` |
| `<stormengine2/tilemapFormat.h>` | the `.map` version header |
| `<stormengine2/version.h>` | the compile-time engine version |
| `<stormengine2/debugOverlay.h>` | fps, slowest system, recent errors |
| `<stormengine2/net/net.h>` | `NetServer`, `NetClient`, snapshots |
| `<stormengine2/net/hostAddress.h>` | the host's own LAN address, ranked |
| `<stormengine2/compat/global.h>` | a 1.x bridge; qualify names or delete it |

`GameState` already includes the engine headers, so a state header does not
need to repeat them.

On Windows the engine is not installed anywhere — the examples link
`build/win/libstormenginev2.dll` from the repo, and a game outside the repo
links the SDK zip attached to a release. See `README.md`.

See `examples/shooter/` for a complete action game example and `examples/puzzle/` for a Tetris-style example using custom components and systems.
