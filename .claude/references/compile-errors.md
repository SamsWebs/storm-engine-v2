# Compile and link errors — Storm! Engine v2

Real compiler and linker output mapped to cause and fix. Match on the message,
not on intuition: several of these have a misleading first reading.

Entries marked **(observed)** were reproduced on this codebase while building
the scaffold in `references/new-game-scaffold/`.

---

## Stale install: the API moved and your headers did not

### `'ContactSystem' was not declared in this scope`

### `'class AssetStore' has no member named 'AddFont'` / `'GetFont'` / `'AddSound'` / `'GetSound'`

### `'CapFrameRate' was not declared in this scope`

### `stormengine2/text.h: No such file or directory`

### `stormengine2/input/gamepad.h: No such file or directory`

All five arrived in **v1.3.0**. Your `/usr/local/include/stormengine2/` predates
it.

```bash
ls /usr/local/include/stormengine2/systems/contact.h   # missing == stale
pkg-config --modversion stormengine2                   # 1.3.0 or nothing
cd /path/to/storm-engine-v2 && make -f Makefile.debian target && sudo make -f Makefile.debian install
```

`ContactSystem` also reaches you transitively through
`<stormengine2/states/gameState.h>` (it includes `systems/collision.h`, which
includes `contact.h`), so a missing declaration means stale headers rather than
a missing `#include`. `text.h` and `input/gamepad.h` are **not** transitive, so
include them yourself.

### `'class Registry' has no member named 'DoesTagExist'; did you mean 'DoesGroupExist'?` **(observed)**

`DoesTagExist`, `IsAlive` and `TryGetComponent` arrived in **v1.2.2**. The
suggestion is a red herring — `DoesGroupExist` is a different call for groups,
not tags. Your `/usr/local/include/stormengine2/` predates 1.2.2.

```bash
grep -c TryGetComponent /usr/local/include/stormengine2/ecs.h   # 0 == stale
cd /path/to/storm-engine-v2 && make -f Makefile.debian target && sudo make -f Makefile.debian install
```

### `no matching function for call to 'RenderSystem::Update(SDL_Renderer*&, AssetStore&, std::nullptr_t)'` — `candidate expects 2 arguments, 3 provided` **(observed)**

Same cause. Camera-aware rendering added the third parameter; the pre-1.2.2
signature is `Update(SDL_Renderer*, const AssetStore&)`. Reinstall rather than
dropping the argument — deleting it silently gives up scrolling.

### `libtinyxml2.so.8 => not found` in `ldd` output **(observed)**

The installed `.so` was built against a tinyxml2 soname the system no longer
ships (Debian/Ubuntu moved to `.so.10`). Rebuilding the engine relinks it
against the current one. Confirm what you have with `ldconfig -p | grep tinyxml2`.

---

## Arity and signature traps that are not staleness

### `no matching function for call to 'RenderColliderSystem::Update(SDL_Renderer*&, std::nullptr_t)'` **(observed)**

A stale install. `RenderColliderSystem::Update` takes
`(SDL_Renderer *, const SDL_Rect *camera = nullptr)` — the overlay pans like
`RenderSystem`. It genuinely took only the renderer up to 2.1.x, so this error
against a current source tree means the installed headers are older than the
tree; reinstall the engine. Against an engine that really is 2.1.x or earlier,
drop the camera argument.

### `undefined reference to 'Registry::AddEntityToSystem(Entity)'`

Only on pre-1.2.x sources, where `Registry::AddEntityToSystem` (singular) was
declared with no definition anywhere — the header offered a symbol the library
never contained. The real entry point is `AddEntityToSystems` (plural). Note
`System::AddEntityToSystem` (singular, on `System`) does exist and is a
different function.

To move a live entity between systems, use both halves:

```cpp
registry.RemoveEntityFromSystems(e);
registry.AddEntityToSystems(e);
```

Calling `AddEntityToSystems` alone double-adds the entity to every system it
already matched.

### Sprite renders as a strip of the tileset's top row, and tiles are stuck to the screen

Not a compile error — an argument-binding bug that compiles cleanly. Omitting
the `isFixed` argument to `SpriteComponent` binds `srcX` to the bool and `srcY`
to `srcRectX`. This shipped in the tanks example. Count the arguments against
the header when a sprite samples the wrong cell.

---

## Build system

### `Package stormengine2 was not found in the pkg-config search path`

The `.pc` file is v1.3.0+. Either the install predates it, or it went somewhere
`pkg-config` does not look. `make install` writes it to
`$(PREFIX)/lib/pkgconfig/stormengine2.pc`, so a non-default `PREFIX` needs
`PKG_CONFIG_PATH` set to match.

```bash
ls /usr/local/lib/pkgconfig/stormengine2.pc
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

Linking only `-lstormenginev2` by hand is what the `.pc` exists to replace: the
linker will not let a game borrow the engine's transitive libraries, so the
moment the game calls SDL directly you get

```
undefined reference to symbol 'SDL_Init'
libSDL2-2.0.so.0: error adding symbols: DSO missing from command line
```

Name them yourself, or use `$(pkg-config --cflags --libs stormengine2)`.

### Bare `make` at the repo root fails

There is no default `Makefile` — name one:

```bash
make -f Makefile.debian      # Linux .so + spec suite
make -f Makefile.win         # MinGW-w64 cross-build, DLL + tests.exe under Wine
```

### `/usr/bin/ld: cannot find -llua` **(observed)**

On a tree predating the fix, `base.mk` passed `-llua`. Debian and Ubuntu ship
`liblua5.4.so` with no versionless `liblua.so`, so the link failed at
`test-target` and the default goal never reached the library build. Installing
`liblua5.4-dev` does **not** help — it provides the versioned soname only.

Lua was never used: no Lua header is included and no `lua_`/`luaL_` symbol is
referenced anywhere in `common/`, `specs/` or `editor/`. The fix is to drop the
flag from `base.mk`, not to symlink a `liblua.so` into place.

### `/usr/bin/ld: cannot find -lnfd`

NFD is editor-only and vendored (`vendor/nfd/nfd.h`); only
`editor/src/utilities/FileDialogWin.cpp` uses it. `base.mk` keeps it out of the
shared `LIB` entirely and in a separate `EDITOR_LIB` that only `editor/Makefile`
puts on a link line, so nothing else should ever see this. Seeing it from a
*game* build means the game inherited the engine's link flags from somewhere:
link SDL2 and `-lstormenginev2` yourself (as the scaffold's Makefile does), or
use `pkg-config --libs stormengine2`.

### `Package gtk+-3.0 was not found by pkg-config`

`base.mk` links GTK3 unconditionally, even for headless examples, so the engine
build needs it present. (`-llua` was dropped, see the entry above, and `-lnfd`
now lives in `EDITOR_LIB`.)

```bash
sudo apt install libgtk-3-dev
```

A **standalone game** does not need it; see the scaffold's Makefile, which
links SDL2 and the engine only.

### Edits to a header have no effect / stale objects

Fixed in v1.2.2: `base.mk` now compiles with `-MMD -MP` and `-include`s the
generated `.d` files, so dependents rebuild. If you are on an older tree, or
your own Makefile lacks those flags, you must `clean` between builds. The
scaffold's Makefile has them.

### `make clean` in an example deleted every other example's objects

Working as written, and a known hazard. `base.mk` derives `ROOT_DIR` from its
own realpath, so `clean` has repo-wide blast radius. Never reuse the engine's
`clean` target in a standalone game.

### `sudo make -f Makefile.debian install` installed an old library

`install` has no prerequisites. Always:

```bash
make -f Makefile.debian target && sudo make -f Makefile.debian install
```

---

## Runtime failures that look like build problems

### The level renders blank, no error, no crash

`TileMapLoader` constructs successfully even when the file is missing,
unreadable, or parses to nothing. Failures go to `Logger::Err`, but `getMap()`
returns an empty `Map` that is indistinguishable from a genuinely empty level.

```cpp
TileMapLoader loader("assets/tilemaps/level.map", "", 32);
if (loader.getMap().empty()) return false;
```

### A sprite is invisible while its neighbours render fine

`SpriteComponent`'s `srcRectX` defaults to `0`. If cell 0 of the sheet is
transparent, that sprite silently draws nothing while every other sprite works
— no error, no log line, and the game looks half-finished rather than broken.
Observed: a generated Pong where the right paddle and ball rendered and the
left paddle did not, because it took the default cell.

Check the cell's alpha before blaming the code:

```bash
python3 -c "from PIL import Image; im=Image.open('assets/gfx/tileset.png').convert('RGBA'); print(sum(1 for p in im.crop((0,0,32,32)).getdata() if p[3]>0),'opaque px in cell 0')"
```

### A sprite renders stretched or samples the wrong part of the sheet

`width` and `height` are **the source rectangle**, not just the draw size — the
constructor builds `srcRect{srcRectX, srcRectY, width, height}` and
`RenderSystem` derives the destination from `sprite.width * transform.scale.x`.
Sizing a sprite by passing the on-screen dimensions therefore reaches outside
the sheet: a 16x90 paddle from a 32x32 cell asks for 90 rows of a 32-row
texture. SDL clamps rather than failing, so it renders stretched.

Pass the **cell** size and resize with the transform:

```cpp
// 16x90 paddle drawn from a 32x32 cell
e.AddComponent<SpriteComponent>("tiles", 32, 32, 1, false, /*srcRectX=*/32, 0);
e.AddComponent<TransformComponent>(pos, glm::vec2(0.5f, 2.8f), 0.0);
```

### An animated sprite draws nothing, or sits frozen on frame 0

The `vertical` flag does not match the sheet layout. `AnimationSystem` advances
`srcRect.y` when `vertical == true` (the **default**) and `srcRect.x` when it is
`false`. A horizontal strip animated with `vertical = true` walks off the bottom
of the texture; a vertical strip with `false` walks off the right edge. Neither
is an error at any layer -- SDL simply blits an empty region.

Count the sheet's dimensions against the frame size to tell which you have:
128x32 with 32x32 frames is horizontal (`false`), 32x128 is vertical (`true`).

### Sprites do not draw and nothing is logged at the draw site

`AssetStore::GetTexture` returns `nullptr` for a missing id instead of throwing.
Check at load time, next to `AddTexture`, where you still know the path.

### A 1.2.x game crashes in the constructor, or corrupts the heap, after an engine upgrade

Not a build problem and not a code bug: **v1.3.0 changed `sizeof(AssetStore)`
from 112 to 208 bytes** when it gained the font and sound caches. Games allocate
the store in their own code (`std::make_unique<AssetStore>()`), so the size is
emitted at the game's call site. A binary compiled against 1.2.x headers
allocates 112 bytes and then calls a 1.3.0 constructor that initialises out to
208. That is a heap overflow, with nothing warning at any layer.

Rebuild the game against the installed headers. Never swap
`libstormenginev2.so` underneath an already-built binary.

### Text draws nothing and reports no error

`Text::Draw` returns `{0, 0}` and draws nothing for a null renderer, a null
font, or an empty string. That is by design, so a missing asset does not crash.
A null
font is what `AssetStore::GetFont` hands back for an id that was never
registered, or whose `AddFont` failed. `AddFont` also fails silently-ish (a
`Logger::Err` line) when `TTF_Init()` was never called: `AssetStore` does not
initialise SDL_ttf or SDL_mixer.

### The process crashes during shutdown, after the game has already exited cleanly

Teardown order. `TTF_Quit()`, `Mix_CloseAudio()` and `SDL_Quit()` free every
open font and chunk themselves, so an `AssetStore` destroyed *after* them hands
already-freed pointers to `TTF_CloseFont` / `Mix_FreeChunk`. Call
`ClearAssets()` first. The same rule applies to `Gamepad`: call `Shutdown()`
before `SDL_Quit()`/`SDL_QuitSubSystem()`, because `SDL_GameControllerQuit`
force-closes every controller.

### Two entities are touching but no contact is reported

`ContactSystem::Overlaps` is **strict**: a shared edge is a zero-area overlap
with no meaningful normal, so it is not a contact. If you are porting logic
that expected an edge touch to count (the old `CollisionSystem`, removed in
2.0.0, was inclusive there), treat this as the correct behaviour to design
around rather than a bug.

The other silent cause is membership - but check the entity's *transform*, not
its collider. `ContactSystem` requires `TransformComponent` alone and re-reads
the collider every frame, so a collider added to a live entity does register. An
entity admitted with no transform is not a member and never becomes one.

### `./bin/tests` fails on missing files

Run it from the repo root — the specs hardcode `./specs/assets/...`.

### The process aborts instead of throwing on a bad lookup

Under the Switch build's `-fno-exceptions`, the ECS paths that use `std::map::at`
(`GetSystem`, `GetEntityByTag`) abort rather than throw. Guard with
`HasSystem()` / `DoesTagExist()` instead of relying on a catch.

### An entity ignores a component you just added

By design. System membership is computed exactly once, when `Registry::Update()`
flushes the entity. `AddComponent` and `RemoveComponent` only flip signature
bits and never re-evaluate membership. Kill and recreate the entity, or use the
`RemoveEntityFromSystems` / `AddEntityToSystems` pair above.

`ContactSystem` and `RenderColliderSystem` are the exceptions, and only
incidentally: both require `TransformComponent` alone and pick the collider up
inside their own `Update()`, so a box or circle collider added after the flush
does take effect. Every other system, and a transform added after the flush,
behaves as described above.
