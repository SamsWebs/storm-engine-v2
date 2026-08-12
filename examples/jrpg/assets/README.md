# Artwork for the JRPG example

Tilesets and character sprites are the **PokeHD JRPG 32x32** pack by
**Monedita**:

<https://monedita.itch.io/pokehd-jrpg-32x32>

- `gfx/Outside-Tileset.png` — grass, the cobbled patch, fences, signs, barrels
- `gfx/Buildings-Tileset.png` — roofs in three colours, facades, and the STORE /
  HEALTH / cross / padlock signs
- `gfx/NPC-Sprite-Sheet.png` — the player and NPC, **32x64** frames on a 640x64
  sheet (20 frames), not 32x32
- `gfx/icon.png` — the window icon: the player's idle-down frame, head and
  torso, scaled 2x. Derived from the sheet above and carries the same terms.

## Licence

**The pack's itch.io page states no licence.** It gives no terms for use,
redistribution, credit or commercial use — the only related line is "If I get
some sell I will love to continue improving this tileset for the people who
allready buy it."

That makes this the one art pack in the repository whose terms are unknown.
Every other example's artwork has something explicit:

| Example | Pack | Terms |
|---|---|---|
| shooter | SpriteLib, Ari Feldman | CPL-1.0, redistribution allowed, licence travels with the files |
| platformer | bee-m tileset | CC0 |
| strategy | Tiny Swords, Pixel Frog | Custom; use allowed, **redistribution forbidden**, so it is not committed |
| **jrpg** | **PokeHD JRPG 32x32, Monedita** | **none stated** |

The files are committed here, which assumes a permission the page does not
actually grant. Worth resolving with the author before this repository is
treated as a redistribution source — either confirming terms, or moving to the
strategy example's model where the pack is downloaded rather than shipped.

## Regenerating the map

`tilemaps/jrpg.map` is generated. Do not hand-edit it — run
`tilemaps/retile.py` from that directory, which is idempotent and rebuilds the
map from whatever is currently in it. `tilemaps/README.txt` holds the original
pack URL.
