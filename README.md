# Storm! Engine v2

Built this engine for game jams and possible future projects.  It's based some of what I've learned from the [SDL Game Development][1] book by [Packt Publishing][2], some of what I've learned from Pikuma's 2D Game Engine series.  The book is a great resource for learning SDL2.  I've made some changes to the engine, including adding a few features, and fixing some bugs.  I've also added some documentation to help you get started with the engine.

## Features

* Support for Windows, Linux, MacOS, and the Nintendo Switch.
* Support for loading maps created with [Tiled][6].

## How to use

For your project, other than the pre-requisite steps, you can either just copy over the `common` and `vendor` folders or compile the engine as a library.  If you want to compile the engine as a library, you can do the following:

```bash
make && sudo make install
```

## Pre-requisites

I've included a Makefile that should work on most Linux systems, including Windows Subsystem for Linux and possibly for MacOS.  You'll need to install SDL2 and the SDL2 image, ttf, and mixer extensions.  To install these extensions on a Debian based system, you can follow [these instructions][4].  If you're on a different system, you'll need to install the SDL2 and extensions [per the instructions for your system][5].  If you already have SDL2 and extensions installed, only steps you need is to install [Tiled][6], [Valgrind][7], and [Igloo][8] if you don't have those already.  On Debian-based systems, you can do the following:

```bash
sudo update && sudo apt install cmake llibtinyxml-dev tiled valgrind

git clone https://github.com/codewars/igloo.git
cd igloo
git submodule add -b headers-only https://github.com/banditcpp/snowhouse snowhouse
git submodule update --init --recursive
mkdir build && cd build
cmake ..
sudo cmake --build .. --target install
```

I'll update these instructions for WSL and MacOS, eventually.  However, if anyone wants to submit a PR for these, I'd be happy to accept it.

## Running

Just simply type `make && make run` to build and run the game.

[1]: https://www.packtpub.com/game-development/sdl-game-development
[2]: https://github.com/PacktPublishing/SDL-Game-Development
[3]: https://www.libsdl.org/
[4]: docs/SDL2-install-instructions.md
[5]: https://wiki.libsdl.org/SDL2/Installation
[6]: http://www.mapeditor.org/
[7]: http://valgrind.org/
[8]: https://github.com/codewars/igloo
[9]: https://switchbrew.org/wiki/Setting_up_Development_Environment
[10]: docs/memory-management.md
