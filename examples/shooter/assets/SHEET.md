# 1945 sprite sheet — cell index

`gfx/sheet.png` — **960x736, 30 cols x 23 rows of 32x32 cells, origin-aligned,
background keyed to transparent.**

```
srcRectX = col * 32
srcRectY = row * 32
```

Cut from SpriteLib's `shooter/1945.png` (1024x768). The original is **not**
usable directly: cells are on a 33px pitch with 1px grey separators starting at
(4,4), and there is no alpha — sprites sit on opaque blue `(0,67,171)`. The
slice removed the separators, re-packed to a clean 32px pitch, and keyed out
`(0,67,171)`, `(0,0,0)` and `(191,191,191)`.

## License

SpriteLib is **Common Public License 1.0**, © 1996-2017 Ari Feldman. Not public
domain. Keep `license.rtf` alongside these files and state the license wherever
they ship.

---

## Fighters — rows 0-4, cols 0-7

Five colour variants of the same aircraft, each an **8-frame banking/roll
animation** (the 1942 evasive roll). Frame 0 is level, flying up-screen.

| Row | Colour | Suggested role |
|-----|--------|----------------|
| 0 | orange / red | **player** |
| 1 | blue | enemy or player 2 |
| 2 | green | enemy |
| 3 | silver / white | enemy |
| 4 | camo green | enemy |

Because these are frames of one roll, they are **not** directional facings.
Frame 0 is the normal flying pose — use it alone unless you are animating a
roll. Playing all 8 in a loop reads as a continuous barrel roll.

Cells at col 8 on these rows are unrelated smaller planes, not part of the roll.

## Bullets and pickups — rows 5-8

| Cells | Content |
|-------|---------|
| r5 c0-c1 | orange bullet pairs (player shot) |
| r5 c2-c7 | **small explosion, 6 frames** — puff to smoke ring |
| r6 c0-c2 | small bullets / single rounds |
| r6 c3 | diamond pickup |
| r6 c4-c7 | small escort planes |
| r7 c0-c5 | elongated bullets / beams |
| r7 c6-c7, r8 c6-c7 | POW power-up markers |
| r8 c0-c5 | formation-marker planes, shield and wing badges |

## Large explosions — rows 9-10

Five frames of a big fireball, each **2x2 cells (64x64)**, at
`(col, row) = (0,9), (2,9), (4,9), (6,9), (8,9)`.

To use these, either set `SpriteComponent(id, 64, 64, z, false, col*32, 9*32)`
and drive frames by hand, or leave them alone and use the 32x32 explosion at
r5 c2-c7 instead — simpler, and the size difference is fine for small enemies.

## Enemy fighters — row 11, cols 0-7

Eight silver/grey aircraft. Unlike rows 0-4 these read as distinct enemy types
rather than roll frames. Cell r11 c8 is a water/terrain texture tile.

## Rows 12 and beyond — inspect before use

Twin-fuselage bombers (boss-scale, spanning multiple cells), more fighters,
terrain and island tiles, plus UI: digits, an alphabet, "GAME OVER", "GET
READY!", menu text and the 1945 title logo.

These are **not** documented cell by cell. They are larger than one cell and
irregularly placed, so measure any you want before wiring them up:

```bash
python3 -c "from PIL import Image; im=Image.open('assets/gfx/sheet.png'); im.crop((0,12*32,320,16*32)).resize((640,256)).save('/tmp/look.png')"
```

## What this sheet does not have

- **No directional facings.** Aircraft point up-screen only. A vertical scroller
  does not need more; do not try to rotate them.
- **No separate enemy-bullet art.** Reuse the player bullets tinted by
  `SDL_SetTextureColorMod`, or just use a different cell.
- **No background/scenery strip.** The water tile at r11 c8 is the only terrain
  in the documented region.

---

## UI elements — separate images in `gfx/`

The sheet's UI text is **not** on the 32x32 grid: it is small bitmap text at
arbitrary offsets, with non-uniform glyph widths. Rather than compute per-glyph
`srcRect`s, it has been cut into discrete images. Blit these whole.

| File | Size | Content |
|------|------|---------|
| `ui_digits.png` | 120x14 | digits 0-9, **repacked to a uniform 12px pitch** — digit `n` is at `srcRectX = n * 12`, `w = 12`, `h = 14` |
| `ui_score_label.png` | 58x17 | `SCORE:` |
| `ui_wave_label.png` | 51x13 | `WAVE:` |
| `ui_menu.png` | 131x83 | the five menu options with a `>` cursor on the first line |
| `ui_logo.png` | 113x58 | the 1945 title panel |
| `ui_game_over.png` | 94x13 | `GAME OVER` |
| `ui_get_ready.png` | 96x13 | `GET READY!` |

### Menu line geometry

`ui_menu.png` is one image containing all five options, top to bottom:

| Option | y within the image |
|--------|--------------------|
| PLAY GAME | 0 |
| OPTIONS | ~17 |
| CREDITS | ~34 |
| SCORING | ~51 |
| QUIT | ~68 |

Lines are ~13px tall on a ~17px pitch. The `>` cursor is baked into the
PLAY GAME line at the left. To show selection on another line, draw your own
marker (a `SDL_Rect` or a small sprite) at that line's y, or crop the cursor off
and blit it separately.

### Rendering a number

Right-to-left, one digit at a time:

```cpp
void DrawNumber(SDL_Renderer *r, SDL_Texture *digits, int value, int x, int y) {
    char buf[16];
    const int n = snprintf(buf, sizeof buf, "%d", value);
    for (int i = 0; i < n; ++i) {
        SDL_Rect src{ (buf[i] - '0') * 12, 0, 12, 14 };
        SDL_Rect dst{ x + i * 12, y, 12, 14 };
        SDL_RenderCopy(r, digits, &src, &dst);
    }
}
```

### Life icons

There is no separate life icon — the small planes in that row touch each other
and cannot be cleanly separated. Use the player fighter, sheet cell (0,0),
drawn small: `SpriteComponent("sheet", 32, 32, z, true, 0, 0)` with
`TransformComponent.scale = (0.5, 0.5)`.

## Sprite facing

**Every aircraft on the sheet points up-screen.** Enemies that fly *down* must
be flipped, or they appear to fly backwards:

```cpp
enemySprite.flip = SDL_FLIP_VERTICAL;
```

This is not optional for a vertical shooter and it is easy to miss, because the
sprite looks fine sitting still.
