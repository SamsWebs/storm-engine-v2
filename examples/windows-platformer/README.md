# windows-platformer

The acceptance test for the shipped Windows SDK.

Every other example builds against the **repository** — `examples.win.mk` points
`-I` at `common/` and `-L` at `build/win`. This one builds against the
**unzipped release zip**, the way somebody who downloaded it does. Nothing else
in the tree does that, so nothing else can tell you the package is wrong.

## It has no sources of its own

It compiles `../platformer/src`. A copy would have been the obvious way to make
a "windows-platformer" and it would rot: two copies of one game, and the one
nobody plays is the one that silently stops matching. What is demonstrated here
is not a different game, it is a different way of **consuming** the engine — so
the game is byte-identical to the example it mirrors.

## Running it

```sh
make -f Makefile.win dist            # from the repo root — produces the zip
cd examples/windows-platformer
make                                 # unzips the SDK, builds against it
make run                             # under wine
```

To build against an SDK you already unpacked:

```sh
make SDK=/path/to/stormengine2-2.1.1-win64
```

## What it has already caught

Both of these shipped in the zip and were invisible until something consumed it:

- **No SDL2 import libraries.** `lib/` held only `libstormenginev2.dll.a`. The
  README tells a consumer to call SDL directly and every real game does, but a
  program cannot borrow its library's transitive imports — so the first build
  failed with `cannot find -lSDL2_image`.
- **The engine DLL re-exported `_Unwind_Resume`.** It was linked
  `-static-libgcc`, which absorbs libgcc's unwinder, and MinGW exports every
  symbol by default. Any consumer also linking `-static-libgcc` got
  `multiple definition of '_Unwind_Resume'`. Invisible for as long as nothing
  linked the DLL — the spec suite links the engine *objects*, and the other
  examples used to compile the engine into themselves.

Keep it that way: this example must only ever see `$(SDK)`. If it grows an `-I`
into `common/` or a `-L` into `build/win`, it stops being a test of anything.
