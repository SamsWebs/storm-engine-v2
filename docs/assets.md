# Assets

How a path becomes a file, how a path becomes a pack entry, and how a player
replaces one of your files. Added 2.5.2, from the roadmap item that noted the
convention was only discoverable by reading `AssetStore`'s source — and then
reimplemented wrongly by a game.

Everything here is `<stormengine2/assetPath.h>`: `AssetPath`,
`AssetFilePath`, `AssetOverridePath`, `AssetOverrideExists`,
`ResolveAssetFile`, `JoinAssetPath`. Header-only, and the path rules are
spec'd in `specs/assetPath.spec.cpp`.

## One path, three bases

The thing to internalise is that there is **one** asset path, not several that
happen to agree. It is relative to the assets root, and it is simultaneously:

- the game's logical asset path, and
- the pack's entry name,

because a pack's entries *are* relative to the assets root. Two functions for
one concept is what produced the bug below.

```
"assets/gfx/player.png"      what a game writes
         |  AssetPath()
         v
"gfx/player.png"             the pack entry name == the logical asset path
         |
         +-- AssetFilePath(.., "assets")      -> "assets/gfx/player.png"
         +-- AssetOverridePath(.., "userdata") -> "userdata/gfx/player.png"
```

`AssetPath()` normalises three things, and the order matters:

1. **Backslashes become forward slashes.** A pack is built on whatever machine
   its author used, and its entry names are forward-slashed whatever built
   them. Without this, a game loads from the pack on the author's Windows
   machine and silently falls back to loose files everywhere else.
2. **A leading `./` is stripped** — repeatedly, since a path can carry more
   than one and all of them are decoration.
3. **A leading `assets/` is stripped — at most once.** A real directory can be
   called `assets`, so `assets/assets/x.png` means `assets/assets/x.png`.

It does **not** resolve `..`. A pack entry name is a key, not a location, and
collapsing the two would let a name address outside the pack.

## Pack versus loose

`AssetStore::LoadPack("assets.pak")` is called once. After that, every
path-based `AddTexture` / `AddFont` / `AddSound` / `ReadBlob` works out its
pack entry name from the path you passed and:

- **pack hit** — the bytes come from the pack;
- **pack miss** — the loose file is opened, and the call still succeeds.

That fallback is the point. A development tree with a partial pack, a pack that
failed to load, and a game that has no pack at all all behave the same, and **no
call site changes**. A missing or corrupt pack file is logged and simply leaves
the loose-file contract standing.

The consequence worth internalising: **a broken pack is a performance problem,
not a correctness one.** If assets look wrong, the pack is the first place to
look, and the second is whether your path spells the `assets/` prefix the way
the convention expects.

### The defect that produced this document

`AssetStore::PackEntryName` was **private**. A game could not reuse a convention
it could not see, so a game reimplemented it, spelled one path
`"./assets/gfx/x.png"`, and the strip looked for `"assets/"` at position 0, did
not find it, and the pack was skipped for that asset — with no diagnostic.
Which reads as "the pack is broken", and sends the next person to debug the
pack instead of the path.

`PackEntryName` is now a one-line delegation to `AssetPath`, so the store and a
game's own loader cannot disagree about what a path means.

## Overrides: shipped content is read-only

A game that wants players to replace art, or a mod system, has the same problem
every time: "shipped content is read-only, the player's replacement lives
beside the save" is a convention nobody implements the same way.

`ResolveAssetFile` makes it one call:

```cpp
#include <stormengine2/assetPath.h>

std::string which;
const std::string file = ResolveAssetFile(
    "assets/gfx/player.png",  // what the game normally writes
    "assets",                 // where shipped content is read from
    "userdata/mods",          // where replacements are read from
    &which);                  // "override" or "shipped"
```

It returns the override when one exists at that path and the shipped file
otherwise, and an empty `writableBase` means "no override support" and gets the
shipped file. Nothing is created, written, or required to exist.

**Keep the `which` out-parameter.** A loader that silently prefers a
replacement is a support ticket nobody can answer: the game shows the wrong
art, and nothing records that it looked at a replacement rather than the
original. Two words in a log line is the entire cost of not having that
conversation.

## Adding a load the store does not wrap

For a surface, a pixel-processing pipeline, the window icon, or any loader
`AssetStore` does not have: use `ReadBlob` rather than opening the file
yourself, so pack and loose work the same way.

```cpp
std::vector<uint8_t> bytes;
if (!store.ReadBlob(path, &bytes)) {
    // pack miss, or no pack: read the file yourself
}
```

The bytes are a **copy**, so they outlive the store. Whether they must outlive
the *call* depends on the loader, and the answer is not the same for all of
them:

| what you build from the bytes | decodes | bytes must live until |
|---|---|---|
| a texture (`IMG_Load_RW`) | synchronously | the call returns |
| a sound (`Mix_LoadWAV_RW`) | synchronously | the call returns |
| **a font (`TTF_OpenFontRW`)** | **lazily, at render time** | **the font is closed** |

SDL_ttf keeps the `SDL_RWops` and reads glyphs out of it when it draws. Let the
vector die and you get garbage glyphs or a crash minutes later, with nothing
pointing back at the load. The `AssetStore` keeps pack-loaded fonts' blobs alive
in its own `fontBlobs` for exactly this reason; a game opening its own font has
to do the same, and `docs/ROADMAP.md` §2.5.3 has the measured table.
