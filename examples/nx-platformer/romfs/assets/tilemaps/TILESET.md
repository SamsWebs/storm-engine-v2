# `16x16-platformer.png` — tile index

160×384 px, 16 px tiles → a **10 × 24 grid** (240 cells, 180 non-empty).

`srcX = col * 16`, `srcY = row * 16`. These are the values that go in fields 5
and 6 of an editor-format `.map` line:

```
tiles 16x16-platformer 16 16 <srcX> <srcY> <zIndex> <worldX> <worldY> 1 1 <collider…> <animated…>
```

`worldX`/`worldY` step by 16 (the source tile size), not by the on-screen 40 px —
`TileMapLoader` divides by `tileSize` to get the grid cell and the game
re-multiplies by `TILE_PX`.

The current `platformer.map` uses **6 of the 180** tiles: `(0,0) (16,0) (16,16)
(48,80) (48,96) (64,144)`.

## Layout

The sheet is three palette themes with the same internal arrangement, followed
by shared props and characters.

| Rows | srcY | Theme |
|------|------|-------|
| 0–7 | 0–112 | **Green / spring** — bright grass, blue accents. What the reference screenshot uses. |
| 8–15 | 128–240 | **Autumn / dark** — same shapes, muted palette, orange accents. |
| 16–21 | 256–336 | **Desert** — sand ground, cacti, log platforms, stone frames. |
| 22–23 | 352–368 | Character body parts and misc. |

## Green theme (rows 0–7)

| Cell (col,row) | srcX,srcY | Tile |
|---|---|---|
| 0,0 / 1,0 / 2,0 | 0,0 / 16,0 / 32,0 | Grass ground top — left, middle, right |
| 3,0 – 6,0 | 48,0 – 96,0 | Floating green platform — left, middle ×2, right |
| 0,1 – 2,1 | 0,16 – 32,16 | Dirt fill (under grass) |
| 0,2 – 2,2 | 0,32 – 32,32 | Dirt fill (continues) |
| 3,1 | 48,16 | Wooden wheel |
| 4,1 | 64,16 | Crate |
| 6,1 | 96,16 | Red potion / bottle |
| 3,2 – 6,2 | 48,32 – 96,32 | Rocks and pebbles |
| 7,0 | 112,0 | Wooden crate |
| 7,1 / 7,2 / 7,3 | 112,16 / 112,32 / 112,48 | Blue block / yellow block / yellow `!` block |
| 9,0 / 9,1 | 144,0 / 144,16 | Red block / blue block |
| 8,2 / 9,2 / 8,3 / 9,3 | 128,32 … | Small platform end-caps (blue, red) |
| 0,3 / 0,4 / 0,5 | 0,48 / 0,64 / 0,80 | **Ladder** (repeat vertically) |
| 1,3 | 16,48 | Small green bush |
| 1,4 / 1,5 | 16,64 / 16,80 | Grey rocks (large, small) |
| 2,3 / 2,4 | 32,48 / 32,64 | Tall green block (top, body) |
| 2,5 | 32,80 | Tree stump |
| 3,3 / 3,4 | 48,48 / 48,64 | Tall cyan/blue block (top, body) |
| 3,5 | 48,80 | Red block |
| 4,4 – 7,4, 4,5 – 6,5 | 64,64 … | Character faces (pink/white) |
| 7,5 | 112,80 | Dark cave mouth |
| 0,6 | 0,96 | **Cloud** |
| 1,6 | 16,96 | White block |
| 0,7 | 0,112 | Light-blue sky / water |
| 4,7 / 5,7 | 64,112 / 80,112 | **Bee** (two frames — animation candidates) |
| 6,7 / 7,7 | 96,112 / 112,112 | Green turtle / slime enemy |
| 6,6 | 96,96 | Yellow pipe bracket |
| 7,6 | 112,96 | Brown box with icon |

## Autumn theme (rows 8–15)

Same arrangement shifted down 8 rows (`srcY += 128`): grass top at 8,
dirt at 9–10, ladder/bush/blocks at 11–13, props at 14.

| Cell | srcX,srcY | Tile |
|---|---|---|
| 0,8 – 2,8 | 0,128 – 32,128 | Dark grass ground top |
| 3,8 – 6,8 | 48,128 – 96,128 | Dark floating platform |
| 0,11 / 0,12 / 0,13 | 0,176 / 0,192 / 0,208 | Ladder |
| 4,14 / 5,14 | 64,224 / 80,224 | Turtle enemies |
| 4,15 – 7,15 | 64,240 – 112,240 | **Ghosts** (white) |
| 0,14 / 1,14 | 0,224 / 16,224 | Grey / white blocks |

## Desert theme (rows 16–21)

| Cell | srcX,srcY | Tile |
|---|---|---|
| 0,16 – 2,16 | 0,256 – 32,256 | Sand ground top |
| 3,16 – 6,16 | 48,256 – 96,256 | Sand platform |
| 7,16 | 112,256 | Rounded sand block |
| 0,17 – 1,17, 0,18 – 2,18 | … | Sand fill |
| 3,17 / 4,17 | 48,272 / 64,272 | Cactus sprouts (small) |
| 3,18 / 4,18 | 48,288 / 64,288 | **Cacti** (full) |
| 3,19 – 6,19 | 48,304 – 96,304 | **Log / branch platform** (horizontal run) |
| 5,20 – 7,20 | 80,320 – 112,320 | Log platform (second run) |
| 0,19 – 2,21 | … | Stone ring / frame (3×3 block) |
| 3,20 – 4,21 | … | Stone ring (2×2) |
| 5,21 / 6,21 | 80,336 / 96,336 | Rubble |

## Misc (rows 22–23)

| Cell | srcX,srcY | Tile |
|---|---|---|
| 0,22 / 1,22 | 0,352 / 16,352 | Plain brown / tan blocks |
| 2,22 | 32,352 | Bucket |
| 3,22 / 4,22 | 48,352 / 64,352 | Character head |
| 3,23 | 48,368 | Character body |

## Notes for authoring

- **Colliders** cannot be inferred from the art. Ground tops, dirt fill, and
  platform runs normally want one; clouds, bushes, rocks, cacti and background
  props do not.
- **Animation** likewise: the two bee cells and the character faces are the
  obvious candidates. The engine parses the animation fields but currently
  discards them (`KNOWN_ISSUES.md` §7), so animate tiles from game code.
- Cells listed as blank in the grid are fully transparent — 60 of the 240.
- Regenerate this index with the contact-sheet script if the art changes; the
  coordinates here are read off the 160×384 sheet as committed.
