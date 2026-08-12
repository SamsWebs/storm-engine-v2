# Artwork for the strategy example

**This example is the only one in the repository that does not run straight from
a fresh clone.** It needs a free art pack that has to be downloaded once.

## Why the art is not in this repository

The sprites are **Tiny Swords** by **Pixel Frog**:

<https://pixelfrog-assets.itch.io/tiny-swords>

The pack is free and may be used in personal and commercial projects, but its
licence is explicit that it may not be passed on:

> You may not redistribute, resell, or repackage the assets, even if the files
> are modified.

Committing the PNGs here would be redistribution, so the example ships code
only. The download is free and takes a minute, and going to the source means the
artist gets the traffic — which is the right outcome for work this good.

## Getting the art

1. Download the **free pack** from the link above.
2. Extract it.
3. Copy files into this directory as laid out below. The names on the right are
   what the game loads; the paths on the left are where they sit in the
   download.

This is the complete list — the game loads exactly these fourteen files and
nothing else.

```
Tiny Swords (Free Pack)/                     ->  examples/strategy/assets/

Units/Blue Units/Warrior/Warrior_Run.png     ->  gfx/units/blue/Warrior_Run.png
Units/Blue Units/Warrior/Warrior_Attack1.png ->  gfx/units/blue/Warrior_Attack1.png
Units/Blue Units/Archer/Archer_Run.png       ->  gfx/units/blue/Archer_Run.png
Units/Blue Units/Archer/Archer_Shoot.png     ->  gfx/units/blue/Archer_Shoot.png

Units/Red Units/...  (the same four files)   ->  gfx/units/red/...

Buildings/Blue Buildings/Castle.png          ->  gfx/buildings/blue/Castle.png
Buildings/Red Buildings/Castle.png           ->  gfx/buildings/red/Castle.png
Buildings/Yellow Buildings/Castle.png        ->  gfx/buildings/neutral/Castle.png

Terrain/Tileset/Tilemap_color1.png           ->  gfx/terrain/tilemap.png
Terrain/Tileset/Water Background color.png   ->  gfx/terrain/water.png

Terrain/Resources/Wood/Trees/Tree3.png       ->  gfx/deco/tree.png
Terrain/Decorations/Bushes/Bushe1.png        ->  gfx/deco/bush.png
Terrain/Decorations/Rocks/Rock1.png          ->  gfx/deco/rock.png

Particle FX/Explosion_01.png                 ->  gfx/fx/explosion.png
```

Yellow stands in for the neutral castles; any unused colour works.

A missing file is fatal and reported, never a black window. `Game::LoadAssets`
checks for `gfx/units/blue/Warrior_Run.png` before loading anything, then
refuses to start if any texture failed — both of the engine's failure paths here
are otherwise quiet (`AssetStore::AddTexture` logs and continues, and
`GetTexture` returns `nullptr` at the point of use). The process exits non-zero,
so `make run` and CI see the failure too.

## What *is* committed

Everything in this directory that is not under `gfx/`:

- **`ui/`** — the digit strip and every text label. The engine has no text
  rendering at all: `common/` holds five components and five systems, and
  nothing links SDL_ttf. Tiny Swords ships buttons and banners but no font, so
  these are rendered from DejaVu Sans by `gen-ui.sh` and committed. Being
  generated, they carry no third-party licence.
- **`gen-ui.sh`** — regenerates `ui/`. Run it after changing any wording. Needs
  ImageMagick.
- **`maps/overworld.map`** — the campaign map, in `TileMapLoader`'s CSV format.

### Reading the tilemap indices

`Tilemap_color1.png` is 576x384: a **64px** grid, nine columns wide, so a tile
index resolves as `row = index / 9`, `column = index % 9`. Pass 64 as
`TileMapLoader`'s third argument — it defaults to 32.

The land tiles form a 3x3 island autotile. The roles below were read off the
alpha channel rather than guessed: a tile's transparent sides are the sides that
face water, and only index 10 is closed on all four, so it is the *only* tile
that can be used for open ground.

```
 0  top-left      1  top        2  top-right
 9  left         10  interior  11  right
18  bottom-left  19  bottom    20  bottom-right
```

Index 4 is empty; the map uses it for water, and the game draws the water tile
underneath everything.

Indices 3, 12, 21 and 27-30 are the degenerate one-tile-wide and one-tile-tall
variants. Using them as ordinary edges — which is the easy mistake — produces a
seam around every tile instead of one continuous coastline.
