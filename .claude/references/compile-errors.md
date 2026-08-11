# Compile and link errors — Storm! Engine v2

Real compiler and linker output mapped to cause and fix. Match on the message,
not on intuition: several of these have a misleading first reading.

Entries marked **(observed)** were reproduced on this codebase while building
the scaffold in `references/new-game-scaffold/`.

---

## Stale install: the API moved and your headers did not

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

Not a stale install — `RenderColliderSystem::Update` genuinely takes **only**
the renderer. The debug collider overlay is not camera-aware, unlike
`RenderSystem::Update`. Drop the camera argument.

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
`editor/src/utilities/FileDialogWin.cpp` uses it. `Makefile.debian` already
strips it from the library and test builds with
`LIB := $(filter-out -lnfd,$(LIB))`. Seeing this error from a *game* build means
the game inherited the engine's `LIB` — link SDL2 and `-lstormenginev2` only, as
the scaffold's Makefile does.

### `Package gtk+-3.0 was not found by pkg-config`

`base.mk` links GTK3 and Lua unconditionally, even for headless examples, so
the engine build needs them present:

```bash
sudo apt install libgtk-3-dev liblua5.4-dev libnfd-dev
```

A **standalone game** does not need any of them — see the scaffold's Makefile,
which links SDL2 and the engine only.

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
