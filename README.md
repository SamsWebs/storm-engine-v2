# Storm! Engine v2

A lightweight, ECS-based 2D game engine built on SDL2 — made for game jams and personal projects.

![Storm Engine v2 platformer example](docs/screenshot.png)
<!-- TODO: replace with an actual screenshot or GIF of your game -->

## Features

- **Entity-Component-System (ECS)** architecture
- **Sprite rendering** with camera, z-index sorting, and flip support
- **Tilemap support** — load maps painted with the built-in tile editor (`.map` format, auto-detected)
- **Box collider** components with debug overlay
- **Asset store** for textures, fonts, and audio
- **Game state machine** for managing scenes
- **Logger** utility
- **Lua scripting** support
- Built-in **tile map editor** with drag-to-paint, drag-to-erase, and layer support
- Example games: platformer, shooter, strategy, puzzle, JRPG, sports

## Installation

Pre-built `.deb` packages are available on the [Releases](https://github.com/WillSams/storm-engine-v2/releases) page.

### Debian / Ubuntu / Linux Mint (amd64)

```bash
sudo dpkg -i libstormenginev2_<version>_amd64.deb
```

#### Raspberry Pi 4/5 with 64-bit OS (arm64)

```bash
sudo dpkg -i libstormenginev2_<version>_arm64.deb
```

After installing, link your project with `-lstormenginev2`.

## Building from Source

### Prerequisites

Install SDL2 and its extensions, plus a few other dependencies:

```bash
sudo apt update && sudo apt install -y \
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev \
    libglm-dev libtinyxml2-dev liblua5.4-dev libgtk-3-dev \
    valgrind
```

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

Swap `platformer` for `shooter`, `strategy`, `puzzle`, `jrpg`, or `sports` to try the others.

## Using the Tile Editor

```bash
cd editor
make && make run
```

- **Left-click / drag** — paint tiles
- **Right-click / drag** — erase tiles
- **D** — toggle collider debug overlay
- The editor saves `.map` files that `TileMapLoader` can load directly in your game

## Windows / WSL

The Makefile should work under WSL2 with the same `apt` prerequisites above. Native Windows builds are not officially supported yet — PRs welcome.

## Credits

Inspired by the [SDL Game Development](https://www.packtpub.com/game-development/sdl-game-development) book (Packt Publishing) and [Pikuma's 2D Game Engine course](https://pikuma.com/courses/cpp-2d-game-engine-development).

[SDL2](https://wiki.libsdl.org/SDL2/Installation) · [GLM](https://github.com/g-truc/glm) · [Igloo](https://github.com/codewars/igloo) · [TinyXML2](https://github.com/leethomason/tinyxml2) · [ImGui](https://github.com/ocornut/imgui) · [Lua](https://www.lua.org/)
