# Editor Tutorial — Building TestProject from Scratch

This tutorial walks through recreating `example_map/TestProject` step by step. By the end you will have painted tiles from two tilesets onto a canvas and saved a project that the storm-engine-v2 loader can open.

---

## 1. Launch the Editor

From the `editor/` directory:

```bash
make run
```

You will see a black canvas with a checkerboard grid and a menu bar across the top.

---

## 2. Set Up the Canvas

TestProject uses a 704×512 canvas with 16-pixel tiles.

1. Open **Edit** in the menu bar.
2. Set **Tile Size** to `16`.
3. Set **Canvas Width** to `704`.
4. Set **Canvas Height** to `512`.

The checkerboard redraws to reflect the new dimensions.

---

## 3. Load the Tilesets

TestProject uses two tilesets. Load them both before painting.

### Zelda Overworld

1. Go to **Tools → Add Tileset**.
2. Navigate to `bin/assets/adv/Tilemaps/Tiles/` and select `Zelda_overworld.png`.
3. The tileset appears in the **Texture** floating window.

### Dungeon Tiles

1. Go to **Tools → Add Tileset** again.
2. Select `Dungeon_Tiles.png` from the same folder.

You can switch the active tileset at any time from the **Tile Properties** window's dropdown.

---

## 4. Enable Tile Painting

Go to **Tools** and check **Create Tiles**.

Two floating windows appear:

- **Texture** — the tileset image at 2× scale. Click a tile here to select it as the source rectangle.
- **Tile Properties** — controls for scale, layer, collider, and animation.

Make sure **Grid Snap** is checked under **Tools** so tiles snap to the 16-pixel grid.

---

## 5. Paint with Zelda Overworld

Switch the **Tile Properties** dropdown to `Zelda_overworld`.

The `.map` file records each tile as:

```
tiles <asset_id> <src_w> <src_h> <src_x> <src_y> <layer> <dst_x> <dst_y> <scale_x> <scale_y>
      <has_collider> [<box_w> <box_h> <off_x> <off_y>]
      <is_animated> [<num_frames> <frame_speed> <vertical> <looped> <frame_offset>]
```

The three Zelda tiles in TestProject all use the top-left tile of the sheet (src 0,0) at 1× scale:

| Destination X | Destination Y |
|---|---|
| 245 | 169 |
| 92 | 309 |
| 204 | 385 |

To place each one:

1. Click the top-left tile in the **Texture** window (src X=0, Y=0).
2. Left-click on the canvas at the target position. Use the **Grid [X, Y]** readout in the menu bar to find the right cell — divide the pixel coordinates by 16 to get the grid cell, then click there.
3. Repeat for each position.

---

## 6. Paint with Dungeon Tiles

Switch the **Tile Properties** dropdown to `Dungeon_Tiles`.

Two different source rectangles are used:

**Source (0, 0) — top-left tile:**

| Destination X | Destination Y |
|---|---|
| 253 | 59 |
| 144 | 136 |

**Source (64, 32) — a tile further into the sheet:**

Click the tile at position 64×32 in the **Texture** window. Each click updates the src X/Y shown in **Tile Properties**.

| Destination X | Destination Y |
|---|---|
| 238 | 263 |
| 81 | 398 |

Left-click on the canvas at each position to paint the tile.

---

## 7. Removing a Misplaced Tile

If you place a tile in the wrong spot, **right-click** it on the canvas to remove it. Use **Ctrl+Z** to undo the last action, or **Ctrl+Shift+Z** to redo.

---

## 8. Navigating the Canvas

| Input | Action |
|---|---|
| `W` / `A` / `S` / `D` | Pan the camera |
| Mouse wheel | Zoom in/out (0.4× – 2.2×) |
| `Space` | Reset zoom and pan |

Zoom with the scroll wheel over the canvas (not over an ImGui window) to inspect tile placement.

---

## 9. Save the Project

Press **Ctrl+S** (or **File → Save**).

A file dialog opens. Navigate to `example_map/` and save as `TestProject`. The editor writes:

| File | Contents |
|---|---|
| `TestProject.lua` | Project metadata (canvas size, tile size, tileset paths) |
| `TestProject.map` | Tile placement data |

The `.lua` file looks like this:

```lua
project = {
    assets = {
        [0] = {
            asset_id = "Dungeon_Tiles",
            file_path = "./bin/assets/adv/Tilemaps/Tiles/Dungeon_Tiles.png"
        },
        [1] = {
            asset_id = "Zelda_overworld",
            file_path = "./bin/assets/adv/Tilemaps/Tiles/Zelda_overworld.png"
        }
    },
    maps = {
        [0] = {
            file_path = "./example_map/TestProject.map"
        },
    },
    canvas = {
        canvas_width = 704,
        canvas_height = 512,
        tile_size = 16
    }
}
```

Paths are relative to `editor/`, the directory the editor runs from.

---

## 10. Re-open the Project

Press **Ctrl+O** and select `TestProject.lua`. The editor reads the project metadata, reloads both tilesets, and restores all painted tiles on the canvas exactly as you left them.

---

## 11. Export for Use in-Game

Once your map is ready, go to **File → Save To Lua Table**. This writes a single Lua table file with all tile and collider data in a format that can be loaded directly by your game via `sol2`.

---

## Next Steps

- Add box colliders: go to **Tools → Create Colliders** and left-click the canvas to place invisible collision boxes. See `TestProject_colliders.map` in `example_map/` for an example with colliders.
- Animate tiles: check **Animation** in **Tile Properties** and set **Num Frames** and **Frame Speed** before painting.
- Load multiple maps in one project: add additional entries to the `maps` table in the `.lua` file.
