# Storm! Engine Tilemap Editor

A tilemap editor for storm-engine-v2 built with SDL2 and ImGui. Load tileset images, paint tiles onto a canvas, place box colliders, and save your work as a Lua project file that can be loaded back into your game.

Adapted heavily (lifted) from Dustin Clark's [Jade Map Editor](https://github.com/dwjclark11/TilemapEditor), which he created to build a Zelda-clone game in C++ using SDL2. The editor's core features are intact, but now works on Linux and a few bug fixes.

## Building

From the `editor/` directory:

```bash
make        # build and launch
make run    # launch without rebuilding
```

The binary is written to `bin/editor`, but it runs with `editor/` as the working directory - it loads `fonts/fontawesome-webfont.ttf` and `./assets/mouse_hand.png` from there, which is what both `make` and `make run` do. The sample tilesets and Lua files under `bin/assets/` are starting material for the file dialogs; the clean rule does not touch any of it.

## Interface Overview

The editor uses a menu bar across the top of the window. Below it is the canvas — a checkerboard grid showing where your tiles will be placed.

```
┌─────────────────────────────────────────────────────┐
│  File   Edit   Tools   Grid [X: 3, Y: 2]  Mouse ... │  ← menu bar
├─────────────────────────────────────────────────────┤
│                                                     │
│   (checkerboard canvas)                             │
│                                                     │
└─────────────────────────────────────────────────────┘
```

When **Create Tiles** or **Create Colliders** is active, two additional floating windows appear:

- **Texture** — the loaded tileset image for picking tiles
- **Tile Properties** / **Box Collider Properties** — controls for the tile you are about to place

## Menu Bar

### File

| Action | Shortcut | Description |
|---|---|---|
| New | `Ctrl+N` | Clear the canvas and start a new project |
| Open | `Ctrl+O` | Open a previously saved `.lua` project file |
| Save | `Ctrl+S` | Save the current project (prompts for path on first save) |
| Save As | `Ctrl+Shift+S` | Save to a new file path |
| Save To Lua Table | — | Export tiles and colliders as a Lua table (for use in-game) |
| Exit | — | Close the editor |

### Edit

| Control | Description |
|---|---|
| **Tile Size** | Size of each tile in pixels (minimum 8, steps of 8). Changing this also changes canvas snap increments. |
| **Canvas Width** | Width of the canvas in pixels (minimum 640, steps of one tile size). |
| **Canvas Height** | Height of the canvas in pixels (minimum 480, steps of one tile size). |
| Undo | `Ctrl+Z` — undo the last tile add, remove, or canvas resize |
| Redo | `Ctrl+Shift+Z` — redo the last undone action |

Canvas resize operations are tracked on the command stack and are fully undoable.

### Tools

| Control | Description |
|---|---|
| **Add Tileset** | Open a file dialog to load a PNG tileset image into the editor |
| **Create Tiles** | Toggle tile-painting mode (mutually exclusive with Create Colliders) |
| **Create Colliders** | Toggle collider-painting mode (mutually exclusive with Create Tiles) |
| **Grid Snap** | Snap tile placement to the grid. When off, tiles can be placed at arbitrary pixel positions. |

## Working with Tiles

### 1. Load a Tileset

Go to **Tools → Add Tileset** and select a PNG file. The image is loaded into the **Texture** window and registered in the asset manager under the file's stem name (e.g. `jungle.png` → asset ID `jungle`).

You can load multiple tilesets and switch between them in the **Tile Properties** window.

### 2. Enable Create Tiles

Go to **Tools** and check **Create Tiles**. Two floating windows appear:

**Texture window** — displays the loaded tileset at 2× scale. Click any tile in this window to select it as the source rectangle for painting.

**Tile Properties window** — configure the tile before placing it:

| Property | Description |
|---|---|
| Change tileset | Dropdown to switch between loaded tilesets |
| X Scale / Y Scale | Scale multiplier for the tile on the canvas (1–10) |
| Layer | Render z-index. Higher layers draw on top. |
| Mouse Rect X/Y | Width/height of the tile region to select from the tileset (steps of 8) |
| Box Collider | Attach a box collider to this tile |
| Box Width/Height | Collider size in pixels |
| Box Offset X/Y | Collider offset from the tile's top-left corner |
| Animation | Mark this tile as animated |
| Num Frames | Number of animation frames in the strip |
| Frame Speed | Frames per second |
| Frame Offset | Starting frame offset |
| Vertical | Use a vertical sprite strip instead of horizontal |
| Looped | Loop the animation |

### 3. Paint Tiles

With **Create Tiles** active:

- **Left-click** on the canvas to place a tile at the mouse position
- **Right-click** on a placed tile to remove it
- The status bar in the menu shows **Grid [X, Y]** and **Mouse [X, Y]** coordinates in real time

Enable **Grid Snap** to have tiles snap to the grid automatically. Without grid snap, tiles are placed at the exact pixel position of the mouse.

## Working with Colliders

Go to **Tools** and check **Create Colliders** (this disables Create Tiles).

The **Box Collider Properties** window appears with the same size/offset controls. Left-click on the canvas to place invisible collider boxes that will be saved alongside your tile data.

Toggle collider visibility at any time with **C**.

## Navigating the Canvas

| Input | Action |
|---|---|
| `W` / `A` / `S` / `D` | Pan the camera up / left / down / right |
| Mouse wheel | Zoom in/out (scroll over the canvas, not over an ImGui window) |
| `Space` | Reset zoom and pan to default (centered view) |
| `C` | Toggle collider outlines on/off |

Zoom range is 0.4× to 2.2×, interpolated smoothly.

## Saving and Loading

### Save

**Ctrl+S** (or **File → Save**) writes up to three files next to your project file:

| File | Contents |
|---|---|
| `<name>.lua` | Project metadata: canvas size, tile size, loaded tileset paths |
| `<name>.map` | Tile placement data |
| `<name>_colliders.map` | Collider placement data (written only when the project has colliders) |

### Save To Lua Table

**File → Save To Lua Table** exports everything as a single Lua table file suitable for loading directly in your game via `sol2`.

### Open

**Ctrl+O** (or **File → Open**) opens a previously saved `.lua` project file. The editor reads the project metadata, reloads all tilesets into the asset manager, and restores the tile and collider entities on the canvas.

## Keyboard Shortcuts Reference

| Shortcut | Action |
|---|---|
| `Ctrl+N` | New canvas |
| `Ctrl+O` | Open project |
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z` | Redo |
| `W/A/S/D` | Pan camera |
| `Space` | Reset camera and zoom |
| `C` | Toggle collider visibility |
| Mouse wheel | Zoom in/out |
| Left-click canvas | Place tile or collider |
| Right-click canvas | Remove tile |
