# Storm! Engine v2

A lightweight, ECS-based 2D game engine built on SDL2 - made for game jams and personal projects.

> **"v2" is the second-generation engine.** The current release is **v2.1.0**, which adds circle colliders and pulls the collision math into a header with no ECS behind it - additive, no break. Before it, **v2.0.0** reset the 1.x API freeze: ten breaking changes land together so the traps they fix are gone for good rather than arriving one per release. **Upgrading from 1.x requires a rebuild, not a relink** - four structs changed size in 2.0.0 and `MAX_COMPONENTS` changed meaning without changing any size at all. Moving from 2.0.0 to 2.1.0 changes no layout and needs no rebuild. See [docs/UPGRADING.md](docs/UPGRADING.md) to migrate, [CHANGELOG.md](CHANGELOG.md) for what changed, and [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for what remains.

![Storm Engine v2 platformer example](examples/platformer/screenshot.png)

## Features

- **Entity-Component-System (ECS)** architecture - up to 32 component types per binary ([see below](#component-type-limit))
- **Sprite rendering** with camera, z-index sorting, and flip support
- **Tilemap support** - load maps painted with the built-in tile editor (`.map` format, auto-detected)
- **Box collider** components with debug overlay
- **Contact detection** - AABB overlaps reported as `Contact{a, b, normal, depth}`, with begin/end callbacks and a pair filter that is where layers, masks and sensors live (`<stormengine2/systems/contact.h>`)
- **Asset store** for textures, fonts and sounds
- **Text drawing** - `Text::Draw` / `DrawCentred` / `Measure` over SDL_ttf, header-only and null-safe (`<stormengine2/text.h>`)
- **Game state machine** for managing scenes, with frame pacing built in (`GameState::CapFrameRate()`)
- **Logger** utility
- **Keyboard** input - edge-triggered `IsDown`/`WasPressed`/`WasReleased` over the full `SDL_Scancode` range, header-only and does not poll (`<stormengine2/input/keyboard.h>`)
- **Gamepad** support - an `SDL_GameController` wrapper with edge-detected `Pressed`/`Released` and a configurable stick deadzone (`<stormengine2/input/gamepad.h>`), used by the shooter, strategy and sports examples
- **Virtual gamepad** for touch devices - d-pad + action-button layout, pure and spec'd (`<stormengine2/input/virtualGamepad.h>`), driven by `examples/android-platformer`
- **Action mapping** - bind one game action across the keyboard, gamepad, virtual gamepad and touch at once, with one edge per action rather than four (`<stormengine2/input/actionMap.h>`, new in 2.0.0)
- **UDP networking** - host/join LAN play: reliable + unreliable chunks, kick/ban/timeout, snapshot replication with per-client deltas and a prediction cache (`<stormengine2/net/net.h>`, see [docs/networking.md](docs/networking.md))
- Built-in **tile map editor** with drag-to-paint, drag-to-erase, and layer support
- Example games: platformer, shooter (*1945*, a vertical shoot-'em-up with menu, HUD and controller support), strategy (*Realms*, a *Dragon Force*-style campaign map with pushed side-on battles - artwork downloaded separately, see below), puzzle, JRPG, sports, Android platformer, Switch platformer, and networking demos (netchat, netrepl, netplay-checkers)
- Platforms: Linux, Nintendo Switch (source builds), Android (source builds, verified on hardware); iOS possible via the same SDL layer

## Collision math without the ECS

The collision math is pure geometry and needs none of the engine's entity machinery. `<stormengine2/collision/shapes.h>` includes glm and nothing else from the engine, so a game that uses no `Registry`, no entities and no components can still use it:

```cpp
#include <stormengine2/collision/shapes.h>
using namespace storm;

ContactCircle puck{x, y, 3.0f};
ContactAABB   post{px0, py0, px1, py1};

glm::vec2 normal;
float depth;
if (Manifold(puck, post, normal, depth)) {
  // normal runs from the puck INTO the post; depth is how far they overlap.
  // Applying MinimumTranslation(normal, depth) to the POST separates them -
  // or negate it and apply it to the puck. Applying it to the puck unchanged
  // drives them together, which is the easy mistake.
}
```

One caveat on `MinimumTranslation`: it is a true minimum separation for circles, but **not for box vs box when one box is contained within the other** along the chosen axis. `Manifold` computes the overlap as `min(maxes) - max(mins)`, which for containment is the inner box's own extent rather than the distance to a face, so the vector it gives you will not separate the pair in one step. Resolve deep box overlaps iteratively, or keep bodies from reaching containment in the first place.

Boxes and circles are both supported, in every combination: `Overlaps` and `Manifold` are overloaded for AABB/AABB, circle/circle and circle/AABB (either order). Circles matter for anything round - a puck kept off the boards by its edge, characters pushed apart by a separation radius, a shot glancing off a post. A circle against a box corner produces a diagonal normal; approximating it with a box would snap that to an axis and change how the game feels.

Overlap is **strict** throughout, by one rule: a contact exists when the shapes share a point interior to at least one of them. So a shared edge and a tangent touch are not contacts - the only shared points are on both boundaries - while a circle centred exactly *on* a box edge is one, because the half-disk inside the box is interior to it. A zero-radius point resting on a boundary is not a contact; strictly inside, it is.

`Overlaps` and `Manifold` are guaranteed never to disagree about whether a contact exists. That is structural, not incidental: each shape pair has a single solver and both call it. They were separate expressions once and drifted apart four different ways.

`ContactSystem` (`<stormengine2/systems/contact.h>`) is the ECS-facing layer on top: it builds bounds from components, sweeps for pairs, and reports begin/end contacts through callbacks. Using it means adopting the ECS - but that is a choice about the broadphase, not a precondition for using the math.

Circles reach the ECS sweep too. `CircleColliderComponent` (`<stormengine2/components/circleCollider.h>`) is the round counterpart to `BoxColliderComponent`, and `ContactSystem` pairs the two shapes against each other, so a round puck can ride against square boards without the game running its own broadphase. Give a body one collider or the other; carrying both is a bug, and the box wins.

Two things about `ContactSystem` changed to make that possible. Its `offset` for a circle places the **centre**, not a corner — a circle has no natural corner to anchor, and anchoring its bounding box would make the offset that centres a collider depend on the radius. And its membership requirement is now `TransformComponent` **alone**: a signature is an AND, so it cannot say "box or circle", and splitting circles into a second system would leave a box/circle pair with one side in each and neither system able to see the other's. `Update()` narrows to the entities that actually carry a collider, which costs a per-frame scan over every transform entity, and means `GetSystemEntities()` now reports transform entities rather than colliders. Read `GetContacts()` instead.

One thing falls out of that requirement: a collider added to a live entity now takes effect, where it used to be ignored. Membership is still decided once, but it is decided on the transform, and the narrowing re-reads the collider every frame. An entity admitted with no transform still never joins, and no system other than these two changed.

`RenderColliderSystem` draws both shapes, and resolves them through `ContactSystem`'s own statics — so the debug overlay cannot disagree with the sweep about where a body is or which of two colliders won. It requires `TransformComponent` alone for the same reason `ContactSystem` does, and `Update(renderer, &camera)` pans the outlines the way `RenderSystem` pans a non-fixed sprite. The camera defaults to `nullptr`, so an existing `Update(renderer)` call is unchanged.

## Lighting without shaders

`<stormengine2/lighting.h>` is a two-layer overlay: a warm key layer whose alpha follows a radial falloff, and a cool vignette layer whose alpha is the inverse of the same falloff. Both are built once at quarter resolution, cached, and drawn over the finished frame with two `SDL_RenderCopy` calls.

The pairing is what makes it read as light rather than as a colour filter — a single warm layer is a tint; adding the cool layer where the warm one is absent gives the frame two colour temperatures and a boundary between them, and the eye reads that boundary as illumination. Quarter resolution is why it is cheap, and it costs nothing visually, because the upscale's bilinear filter does the smoothing a full-resolution falloff would have computed per pixel.

No shaders, so it runs on the plain SDL2 renderer — Switch and Android included. Like the collision math, it includes no engine header, so a game with no `Registry` can use it.

```cpp
LightingOverlay lighting;
lighting.Build(renderer, LightingOverlay::Params{});   // once, on enter
...
registry.GetSystem<RenderSystem>().Update(renderer, assetStore, &camera);
lighting.Draw(renderer);                               // last, over the frame
```

Draw it last. It is a post-pass: anything drawn afterwards is not lit, which is what you want for a HUD and never what you want for the world.

## Namespace

Every engine type lives in `namespace storm` as of 2.0.0. Before that they were all global, so a game declaring its own `Entity` or `Logger` collided with the engine's.

New code qualifies or opens the namespace:

```cpp
#include <stormengine2/ecs.h>

using namespace storm;   // what the examples, the editor and the starter template do
```

An existing 1.x game needs no editing **for the namespace change**. `<stormengine2/compat/global.h>` emits a `using` declaration for every public engine name, so the cheapest migration is one line in the build:

```make
CXXFLAGS += -include stormengine2/compat/global.h
```

The bridge covers the namespace and nothing else. 2.0.0 makes eight other breaking changes, and no `using` declaration can bridge a deleted type: `CollisionSystem` is gone, `Entity::operator<` is deleted, `Entity(std::size_t)` is `explicit`, and the networking types are non-copyable. See [docs/UPGRADING.md](docs/UPGRADING.md) for those.

That header is a bridge, not an API. It pulls every engine name back into the global namespace - the exact collision the namespace exists to prevent - so a game that keeps it forever gains nothing from the change. Use it to get green, then drop it and fix the names. A future major removes it.

## Component type limit

An entity's component set is tracked as a bitmask, so the engine supports **64 distinct component types** (32 before 2.0.0):

```cpp
constexpr unsigned int MAX_COMPONENTS = 64;   // common/ecs.h
using Signature = std::bitset<MAX_COMPONENTS>;
```

Two things about that number are easy to get wrong:

**It is per binary, not per `Registry`.** Type ids come from a single process-wide counter, handed out on first use of each distinct `Component<T>`. Every `Registry` you create draws from the same pool of 64, so splitting your world across several registries - one per game state, as the examples do - does not buy you more types. The five components the engine ships (`TransformComponent`, `RigidBodyComponent`, `SpriteComponent`, `BoxColliderComponent`, `AnimationComponent`) count against your budget as soon as you use them, leaving 59 for the game.

**It counts types, not instances.** Ten thousand entities carrying `TransformComponent` use one id. Component types are cheap to instance and expensive to *declare*, so prefer widening an existing component over adding a new one - a `kind` enum inside one component costs nothing, a new struct costs a permanent 1/64.

Declaring a 65th type is reported on the error log and the type is ignored; it does not throw, so it will not abort the Switch build. Since 2.0.0 a system whose requirement was dropped this way is **latched off** and matches nothing, rather than matching every entity as it did before - `System::IsDisabled()` reports it. That is still a bug to fix rather than a degraded mode to ship, but it now fails in the direction a game can survive.

Raising the cap further means editing `MAX_COMPONENTS` and rebuilding **everything** that includes `ecs.h`. Anything less is undefined behaviour: `Signature` is `std::bitset<MAX_COMPONENTS>`, so two translation units compiled with different values disagree about what type `Signature` *is*.

That mismatch is silent up to 64, which is why the 32 → 64 bump needed a major release rather than a point one. `sizeof(std::bitset<N>)` is 8 bytes for every `N` from 1 to 64 and 16 bytes from 65, so the change moved no struct and no size check could catch a stale object file - a game built against a 32-component header and linked to a 64-component `.so` simply misbehaves. `specs/layout.spec.cpp` therefore pins the *value* alongside the sizes, since the sizes cannot see it.

64 is also the last free step. At 65 the bitset doubles to 16 bytes, moving `sizeof(Registry)` and `sizeof(System)` with it - a second ABI break, not a recompile.

## Diagnostics

The engine logs a small, fixed number of occurrences of each of the following misuses at `Err` level - never more than a handful per call site - and then goes quiet. They are on in every build, debug and release alike, so a game that sees one of these in its log has a real bug to fix, not a warning to suppress:

- A component added to an entity **after** it was already admitted by `Registry::Update()`, so it never joins the system that needed it
- A system registered **after** matching entities already exist, so it never sees them
- A `Registry` destroyed having created entities but never having called `Update()` on them, so nothing it owned ever rendered or moved
- A sprite's `srcRect` falling outside its texture, which `SDL_RenderCopyEx` draws as nothing and reports nothing
- `Registry::GetSystem<T>()` about to throw for a system that was never registered
- `Registry::GetComponent<T>()` missing and falling back to a shared default instance
- A **stale `Entity` handle** - one kept past its entity's death, whose id has since been recycled - used to kill, tag, group, add to a system, or read or write a component. It no-ops rather than touching the live entity that now holds that id

Each is throttled independently and stays silent afterward, so it will not flood a game that hits the same misuse every frame.

## Installation

Pre-built `.deb` packages are available on the [Releases](https://github.com/WillSams/storm-engine-v2/releases) page.

**Supported baseline: SDL2 2.0.10 or newer**, which means Ubuntu 20.04 / Linux
Mint 20 and up, and Debian 11 (bullseye) and up. Both architectures are checked
against `ubuntu:20.04`, `debian:bullseye`, `debian:bookworm`, `ubuntu:22.04` and
`ubuntu:24.04` on every release -- the package is installed in each and its
library checked for unresolved libraries, so this is a tested claim rather than
an intention.

### Debian / Ubuntu / Linux Mint (amd64)

```bash
sudo apt install ./libstormenginev2_<version>_amd64.deb
```

> Use `apt`, not `dpkg -i`. **`dpkg -i` does not install dependencies** — on a
> machine without the SDL2 runtime it stops with
> `dependency problems prevent configuration` (or, if you double-click the file,
> *"Error: Dependency is not satisfiable: libsdl2-2.0-0"*). `apt` fetches them.
> The leading `./` is required: without it apt looks for a package by that name
> instead of the file. If you already ran `dpkg -i` and it half-installed,
> `sudo apt-get install -f` finishes the job.


#### Raspberry Pi 4/5 with 64-bit OS (arm64)

```bash
sudo apt install ./libstormenginev2_<version>_arm64.deb
```

### Windows (x64, MinGW-w64)

Download `stormengine2-<version>-win64.zip` from the same Releases page and
unzip it anywhere. There is no installer and nothing to register.

```
g++ -std=c++17 mygame.cpp -Iinclude -Llib -lstormenginev2 -o mygame.exe
```

Then copy the contents of `bin\` beside your `.exe`, or put that directory on
`PATH`. Windows resolves DLLs from the executable's own directory first and
says nothing useful when one is missing, so copy the whole directory rather
than picking out the ones you think you need.

The zip carries the SDL2, glm and tinyxml2 headers as well as the engine's own,
because 12 engine headers include `<SDL2/SDL.h>` and 6 include `<glm/...>` --
on Debian those come from `libsdl2-dev` and `libglm-dev`, and on Windows there
is no package manager to get them from.

> **MSVC is not supported.** The import library and the C++ ABI are GCC's, so a
> program compiled with `cl.exe` cannot link this build. Use MinGW-w64, or
> build from source with your own toolchain.

> **Upgrading from 1.2.x is a rebuild, not a relink.** `AssetStore` grew font
> and sound caches in 1.3.0, so `sizeof(AssetStore)` went from 112 to 208
> bytes. Every game allocates the store in its own code, so a binary compiled
> against 1.2.x headers reserves the smaller size and then calls a 1.3.0
> constructor that initialises out past it. The `.deb` ships the headers and
> the `.so` together, so installing the package and rebuilding the game is
> safe - swapping `libstormenginev2.so` underneath an already-built game is
> not.

### Building a game against the installed engine

The package gives you the library, the headers, a `pkg-config` file and a
starter game. To **compile** against it you also need the development headers
for its dependencies - the package's own `Depends:` covers the runtime
libraries only:

```bash
sudo apt install build-essential pkg-config \
                 libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
                 libsdl2-mixer-dev libglm-dev
```

> `libtinyxml2-dev` is **no longer required and should not be installed for
> this**. The engine compiles tinyxml2 in and ships `tinyxml2.h` inside the
> package, so your game gets the header and the symbols from the engine itself.
> That is deliberate: `XmlLoader` embeds an `XMLDocument` by value, so your
> translation unit emits `~XMLDocument` and has to agree with the engine on the
> layout. Compiling against a system tinyxml2 of a different vintage would be a
> silent ODR violation.

Copy the starter game and build it:

```bash
cp -r /usr/local/share/stormengine2/template ~/mygame
cd ~/mygame
make run
```

Its `Makefile` needs no engine source. To wire the engine into a build of your
own, ask `pkg-config`:

```bash
g++ -std=c++17 mygame.cpp $(pkg-config --cflags --libs stormengine2) -o mygame
```

> Do **not** link with `-lstormenginev2` alone. The moment your game calls SDL
> directly - and every real game does - the linker fails with
> `undefined reference to symbol 'SDL_Init'` /
> `DSO missing from command line`, because it will not let your game borrow the
> engine's transitive libraries. `pkg-config` emits the full set.

## Building from Source

### Prerequisites

Install SDL2 and its extensions, plus a few other dependencies:

```bash
sudo apt update && sudo apt install -y \
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev \
    libglm-dev libgtk-3-dev \
    valgrind
```

**Then fetch the vendored tinyxml2, which the library now compiles in:**

```bash
git submodule update --init vendor/android/tinyxml2
```

Without it the build stops at the first `#include "tinyxml2.h"`. It is under
`vendor/android/` because Android vendored it first, not because Android is the
only user -- the Linux, Windows and Android builds all compile this same pinned
copy now, and `libtinyxml2-dev` is no longer a build dependency on any of them.

Install [Igloo](https://github.com/codewars/igloo) (test framework):

```bash
git clone https://github.com/codewars/igloo.git
cd igloo
git submodule add -b headers-only https://github.com/banditcpp/snowhouse snowhouse
git submodule update --init --recursive
mkdir build && cd build
cmake ..
sudo cmake --build . --target install
```

### Build & install the library

```bash
make -f Makefile.debian
sudo make -f Makefile.debian install
```

### Run the tests

```bash
make -f Makefile.debian test
```

## Running the Examples

```bash
cd examples/platformer
make && make run
```

Swap `platformer` for `shooter`, `strategy`, `puzzle`, `jrpg`, `sports`, or `netplay-checkers` to try the others. `netplay-checkers` is a graphical 2-player game: the host validates every move over the net module and the first two joiners are seated RED and BLACK (`host` starts it with `S`).

Two examples are headless console demos of the networking module - no graphics, run from a terminal. `cd examples/netchat && make && ./bin/netchat host` opens a room; `./bin/netchat join 127.0.0.1 5000` joins it from another terminal. `examples/netrepl` is the same shape (`host` / `join`) and streams snapshot deltas. See each example's README for usage.

### Building an example for Windows

Every desktop example also cross-compiles to a Windows `.exe` from Linux. Build
the vendored Windows dependencies once, then build any example with its
`Makefile.win`:

```bash
make -f Makefile.win deps          # once, from the repo root - slow
cd examples/platformer
make -f Makefile.win && make -f Makefile.win run   # run needs wine64
```

The `.exe` and every DLL it needs - including `libstormenginev2.dll` - land in
`bin/win/`. An example needs only a three-line `Makefile.win` naming itself and
including `../examples.win.mk`, exactly like its Linux `Makefile`.

These build against the **repository**. `examples/windows-platformer` is the one
that builds against the **unzipped release zip**, the way a user who downloaded
it does; see its README. It has no sources of its own - it compiles
`../platformer/src`, because what it demonstrates is a different way of
consuming the engine, not a different game.

Seven of the eleven examples cross-compile. The four that do not are excluded
deliberately:

| Example | Why not |
|---|---|
| `nx-platformer` | targets devkitPro |
| `android-platformer` | targets the Android NDK |
| `netchat` | `select()` on `STDIN_FILENO` for non-blocking keyboard input |
| `netplay-checkers` | `fcntl(O_NONBLOCK)` on stdin for its scripted-input test channel |

The last two are **not** a networking limitation -- the net module cross-compiles
and `netrepl` uses it on Windows. Both examples poll stdin the POSIX way, which
MinGW does not provide for console handles; making them portable means Windows
console-handle APIs and a behaviour change, not a build fix.

### How each example loads its world

Each example demonstrates a different approach to managing game resources - pick whichever matches your project's needs:

#### Platformer / Strategy - tile editor + `.map` file

The level is painted in the built-in tile editor and saved as a `.map` file. At runtime, `TileMapLoader` reads the file and spawns tile entities automatically. This is the most data-driven approach for tilemap games and the best starting point if you want to design levels visually.

```text
editor/ → paint level → saves level.map
examples/platformer/ → TileMapLoader reads level.map at runtime
```

#### Sports / Puzzle - everything in code

No external level files. Entities, positions, sizes, and game rules are all defined directly in the game state. Simple and self-contained - a good starting point for understanding the ECS pipeline without any file I/O in the way.

#### Shooter - XML data via `XmlLoader`

Textures and initial entities are described in an XML file (`assets/attack.xml`). The engine's `XmlLoader` parses the file, and `LoadTexturesFromXml` (a helper in `<stormengine2/xmlLoader.h>`) loads them into the asset store. Entity spawning logic then reads the parsed data and creates ECS entities from it.

```text
assets/attack.xml → XmlLoader → LoadTexturesFromXml → AssetStore
                              → XmlObjectDef list → ECS entities
```

This approach keeps your texture IDs and initial object placement out of code and in data files - useful when designers or tools are generating the XML.

#### JRPG - tile editor `.map` + custom colliders map

The world is painted in the tile editor (`jrpg.map`). A second file, `jrpg_colliders.map`, lists solid tiles using a simple `collider worldX worldY ...` format. At runtime, `TileMapLoader` spawns tile entities (using `tileSize=8` to preserve exact editor pixel coordinates), and a custom parser reads the colliders file into a list of `SDL_Rect` obstacles used for AABB collision.

```text
editor/ → paint level → jrpg.map + jrpg_colliders.map
examples/jrpg/ → TileMapLoader reads jrpg.map
              → custom parser reads jrpg_colliders.map → SDL_Rect list
```

NPC interaction and Final Fantasy-style typewriter dialogue are handled via `NpcComponent` and `DialogueState` - no external scripting required.

| Example | Resource approach | Key engine type |
|---|---|---|
| **Platformer** | `.map` file from the tile editor | `TileMapLoader` |
| **Strategy** | `.map` file from the tile editor | `TileMapLoader` |
| **Sports** | Hard-coded in the game state | - |
| **Puzzle** | Hard-coded in the game state | - |
| **Shooter** | XML file via tinyxml2 | `XmlLoader`, `LoadTexturesFromXml` |
| **JRPG** | `.map` + custom colliders map | `TileMapLoader`, custom parser |
| **Netchat** | Console demo - none | `NetServer`, `NetClient`, `NetMessageWriter` |
| **Netrepl** | Console demo - none | `NetSnapshot`, `NetSnapshotDelta` |
| **Checkers** | Hard-coded in the game state | `NetServer`, `NetClient`, `NetMessageWriter`, `RenderSystem` |

## Using the Tile Editor

```bash
cd editor
make        # build and launch (the link rule runs the binary)
make run    # launch without rebuilding
```

- **Left-click / drag** - paint tiles
- **Right-click / drag** - erase tiles
- **C** - toggle collider debug overlay
- The editor saves `.map` files that `TileMapLoader` can load directly in your game

The editor is the only target that links [NFD](https://github.com/mlabbe/nativefiledialog), which Debian and Ubuntu do not package. Build and install it from source once, or the link fails with `cannot find -lnfd`. Nothing else needs it - the library, the spec suite and every example build without it, which is why CI compiles the editor to objects and stops short of the link.

## Action mapping

Four input sources ship in `<stormengine2/input/>`, and until 2.0.0 nothing tied them together, so a game that supported a keyboard, a controller and a phone wrote this in every state, for every action:

```cpp
if (keyboard.WasPressed(SDL_SCANCODE_SPACE) ||
    gamepad.Pressed(GamepadButton::A) || vpad.a || touch.jump) Jump();
```

`ActionMap` resolves one action across all four:

```cpp
enum class Action { Jump, Left, Right };

ActionBinding jump;
jump.key   = SDL_SCANCODE_SPACE;
jump.pad   = GamepadButton::A;
jump.vpad  = VPadControl::A;
jump.touch = TouchControl::Jump;
actions.Bind(static_cast<int>(Action::Jump), jump);

// once per frame, after feeding the keyboard its events and calling gamepad.Update()
actions.Update(&keyboard, &gamepad, &vpad, &touch);

if (actions.WasPressed(static_cast<int>(Action::Jump))) Jump();
```

Every source is optional - pass `nullptr` and it contributes nothing, so a desktop build and a phone build share one binding table and differ only in what they hand to `Update`.

With more than one source bound to an action, the action goes down when the **first** source takes it and comes up when the **last** one lets go: pressing a second source mid-hold reports no new press, and releasing one of two held sources reports no release.

Keyboard and gamepad edges are taken from those classes rather than recomputed, so a key pressed and released inside a single frame is still seen as a press. Deriving edges from the held state alone would drop fast taps silently - which is why `Keyboard` tracks presses separately in the first place.

## Windows / WSL

Windows is supported via a MinGW-w64 cross-compile from Linux - no Windows toolchain needed. SDL2 and its satellites are cross-built from the same vendored sources the Android build uses (`vendor/android/`), so nothing is downloaded:

```bash
sudo apt install mingw-w64 cmake
make -f Makefile.win         # build/win/libstormenginev2.dll + build/win/tests.exe
make -f Makefile.win test    # run the spec suite under Wine
```

The library and the spec suite cross-build; the examples are not wired into the Windows build yet. WSL2 runs the plain Linux build above (`make -f Makefile.debian`) with the same `apt` prerequisites. Native Windows builds (a toolchain that runs in cmd/PowerShell) are not supported yet - PRs welcome.

## Nintendo Switch

Switch builds use [devkitPro](https://devkitpro.org/) and produce a `.nro` homebrew file.

### Prerequisites

Install devkitPro with the Switch portlibs:

```bash
# Follow the devkitPro pacman setup at https://devkitpro.org/wiki/Getting_Started
sudo dkp-pacman -S switch-dev switch-sdl2 switch-sdl2_image switch-sdl2_ttf switch-sdl2_mixer switch-tinyxml2
```

### Build

```bash
export DEVKITPRO=/opt/devkitpro
export PATH=$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH

cd examples/nx-platformer
make
```

This produces a `.nro` you can run on a homebrew-enabled Switch. To launch in an emulator, use [Suyu](https://git.suyu.dev/suyu/suyu/releases) (the recommended Yuzu successor on Linux). Make sure to set the graphics backend to **OpenGL** in Suyu's settings before running:

```bash
make run EMULATOR=/path/to/Suyu.AppImage
```

Copy the `.nro` to `switch/` on your SD card to run on real hardware via the Homebrew Menu.

### Using Storm Engine in your own Switch project

There are no pre-built Switch packages - the engine is compiled directly into your game. Use `examples/nx-platformer` as your starting point:

1. Copy `examples/nx-platformer` to your new project
2. The `include/stormengine2` symlink points to `common/` - keep it or copy the sources in directly
3. Add your own game states, components, and assets under `src/` and `romfs/`
4. Build with `make` as above

## Android

Android builds compile the engine and your game into a single JNI library via
Gradle + CMake + NDK, hosted by SDL's `SDLActivity`. Verified on real hardware
(arm64) over USB debugging.

### Prerequisites

- Java 17, Android cmdline-tools, NDK, and CMake - full install commands in
  [`examples/android-platformer/README.md`](examples/android-platformer/README.md)
- The pinned dependency submodules:

```bash
git submodule update --init --recursive vendor/android   # SDL2, SDL_image, SDL_ttf, SDL_mixer, tinyxml2, glm
```

### Build

```bash
cd examples/android-platformer
./gradlew assembleDebug      # app/build/outputs/apk/debug/app-debug.apk
./gradlew installDebug       # install onto a USB-debugging device
adb shell am start -n com.stormengine.platformer/.PlatformerActivity
```

The example drives the engine's virtual gamepad (`<stormengine2/input/virtualGamepad.h>`
— circular d-pad plus action diamond, pure and spec'd) on top of the desktop
platformer, letterboxes a fixed logical resolution onto any screen, and extracts
APK assets to internal storage at first launch so the engine's plain-file I/O
works unchanged. It follows the phone through all four orientations even when
the system auto-rotate toggle is off; orientation is a per-game decision made
in the game's own Activity, not by the engine. See the example README for the
controls, the orientation mechanics (SDL overrides the manifest at window
creation), and the build gotchas (shared vs. static SDL, the
`SDL2/SDL_image.h` include shim, adb multi-device).

### Using Storm Engine in your own Android project

Like the Switch, there are no pre-built Android packages - the engine compiles
into your app. Use `examples/android-platformer` as your starting point:

1. Copy `examples/android-platformer` to your new project
2. The `include/stormengine2` symlink points to `common/` - keep it or copy the sources in
3. Change the `applicationId`/package name, add your states under `src/`, point
   the `assets.srcDirs` at your asset folder
4. Build with `./gradlew` as above

## License

Released under the [WTFPL](LICENSE.md) - do what you want with it.

## Credits

Inspired by the [SDL Game Development](https://www.packtpub.com/en-us/product/sdl-game-development-9781849696838) book by Shaun Mitchell and Gustavo Pezzi's [C++ Game Engine Programming course](https://pikuma.com/courses/cpp-2d-game-engine-development).

[SDL2](https://wiki.libsdl.org/SDL2/Installation) · [GLM](https://github.com/g-truc/glm) · [Igloo](https://github.com/codewars/igloo) · [TinyXML2](https://github.com/leethomason/tinyxml2) · [ImGui](https://github.com/ocornut/imgui) · [NFD](https://github.com/mlabbe/nativefiledialog) (editor only)

### Artwork

The `shooter` example uses **SpriteLib**, © 1996–2017 [Ari Feldman](https://widgetworx.com/projects/sl.html),
distributed under the **Common Public License 1.0** - free to use and
redistribute, but not public domain, and its terms travel with the files. The
license is kept beside the artwork in `examples/shooter/assets/license.rtf` and
must stay with it. This differs from the rest of the repository, which is WTFPL.

The `platformer` example's tileset is CC0 from
[bee-m](https://bee-m.itch.io/simple-platformer-tileset-8x8-and-16x16); see
`examples/platformer/assets/tilemaps/License.txt`.

The `strategy` example uses **Tiny Swords** by
[Pixel Frog](https://pixelfrog-assets.itch.io/tiny-swords). That pack is free
for personal and commercial use but **may not be redistributed**, so it is the
one example whose artwork is *not* in this repository: it ships code only and
the pack is downloaded once, following
`examples/strategy/assets/README.md`. Running it without the art prints the
download URL and exits. Crediting Pixel Frog is optional under those terms and
done here anyway.

The `jrpg` example uses **PokeHD JRPG 32x32** by
[Monedita](https://monedita.itch.io/pokehd-jrpg-32x32).

> **That pack states no licence.** Its page gives no terms for use,
> redistribution, credit or commercial use, which makes it the only artwork here
> whose terms are unknown - the three packs above all say something explicit.
> The files are committed, which assumes a permission the page does not actually
> grant. Worth settling with the author before this repository is treated as a
> source to redistribute from; the alternative is the `strategy` example's
> model, where the pack is downloaded rather than shipped. See
> `examples/jrpg/assets/README.md`.
