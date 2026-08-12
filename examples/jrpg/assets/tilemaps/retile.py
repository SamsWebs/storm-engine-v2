#!/usr/bin/env python3
"""Rebuild the ground layer of jrpg.map and remove the misused "wall" runs.

The map is painted freehand: tiles are 32x32 stamps placed on an 8px grid, so
they overlap and the edges are ragged. That is fine for the decorated layers,
which is where the freehand look actually reads as hand-placed detail -- but it
was also true of the ground, where only 210 of 854 stamps landed on a tile
boundary. Everywhere the ground had a gap, the renderer's clear colour showed
through; that colour is (34,85,34), close enough to grass to look like terrain,
so the holes read as dark-green blobs rather than as missing tiles.

What this does:

  * Replaces layer 0 entirely with a clean 32px grass grid covering the whole
    level, with the tufted variant scattered deterministically for texture.
  * Deletes four runs that use props as walls -- barrels fencing the top,
    bottom and right edges, and thirteen stacked signboards partitioning the
    middle of the play area.
  * Lays a picket fence along the top and bottom edges in their place.

Layers 1 and up are passed through untouched: the plaza, the building, the
signs and the scattered props keep their hand-placed positions.

Run from this directory. Writes jrpg.map in place; the previous file is kept as
jrpg.map.bak so a bad run can be undone.

Line format (see common/tilemapLoader.cpp):
  group assetId tileW tileH srcX srcY zIndex worldX worldY scaleX scaleY
  collider [colW colH offX offY] animated [numFrames speed vert loop frameOff]
"""

import shutil
from pathlib import Path

MAP = Path("jrpg.map")

TILE = 32
LEVEL_W = 1248          # keep: 39 columns
LEVEL_H = 640           # was 448, which is shorter than the 600px window --
                        # the camera clamp then pinned y at 0 and the bottom
                        # 152px could never hold anything. 20 rows now.

GRASS = (0, 0)          # plain
GRASS_TUFT = (32, 0)    # same grass with a few blades
FENCE = (160, 64)       # picket pair, already used along the plaza's south side

PLAZA_EDGE_R = (64, 64)  # cobbled border, mirrored from the left edge (0,64)
PLAZA_RIGHT = 368        # origin of the fill's last column
PLAZA_TOP = 96
PLAZA_BOTTOM = 304

# (srcX, srcY, axis, value) -- stamps of this tile on this line are the run.
# Identified by histogramming the map: each is a single prop repeated along one
# edge, which is what makes them read as a wall of barrels rather than scenery.
WALL_RUNS = [
    ((192, 96),  "y", lambda v: v == 0),        # top: 74 x two barrels
    ((160, 128), "y", lambda v: v >= 416),      # bottom: 85 x four barrels
    ((224, 128), "x", lambda v: v >= 1232),     # right: 24 x two barrels
    ((80, 112),  "x", lambda v: v == 384),      # divider: 13 x signboard
]


def hash_cell(x, y):
    """Small integer hash. Deterministic across runs and Python versions --
    Python's own hash() is salted per process, which would make the map churn
    on every regeneration."""
    h = (x * 374761393 + y * 668265263) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return h ^ (h >> 16)


def is_wall(src, wx, wy):
    for tile, axis, test in WALL_RUNS:
        if src == tile and test(wx if axis == "x" else wy):
            return True
    return False


def emit(src, z, wx, wy):
    return (f"tiles Outside-Tileset {TILE} {TILE} {src[0]} {src[1]} {z} "
            f"{wx} {wy} 1 1 0 0")


def main():
    lines = MAP.read_text().splitlines()
    shutil.copy(MAP, MAP.with_suffix(".map.bak"))

    kept, dropped_ground, dropped_wall = [], 0, 0
    for line in lines:
        f = line.split()
        if len(f) < 13:
            continue
        src = (int(f[4]), int(f[5]))
        z = int(f[6])
        wx, wy = int(float(f[7])), int(float(f[8]))

        if z == 0:
            dropped_ground += 1      # rebuilt below
            continue
        if is_wall(src, wx, wy):
            dropped_wall += 1
            continue
        kept.append(line.rstrip())

    ground = []
    cols, rows = LEVEL_W // TILE, LEVEL_H // TILE
    for ry in range(rows):
        for rx in range(cols):
            # Deterministic scatter, so the map is byte-identical on every run
            # and a diff means someone actually changed something.
            #
            # Hashed, not `(rx * a + ry * b) % m`: any linear form puts the
            # chosen cells on a lattice, and at this density the tufts came out
            # as obvious diagonal stripes across the whole field.
            tuft = (hash_cell(rx, ry) % 9) == 0
            ground.append(emit(GRASS_TUFT if tuft else GRASS, 0,
                               rx * TILE, ry * TILE))

    fence = []
    for rx in range(cols):
        fence.append(emit(FENCE, 1, rx * TILE, 0))
        fence.append(emit(FENCE, 1, rx * TILE, (rows - 1) * TILE))

    # The plaza already has a cobbled border along its top (16,32), bottom
    # (16,112) and left (0,64) -- only the right was left as a flat cut through
    # the fill. 64,64 is the same border mirrored; drawn on layer 2 so it covers
    # the fill's last column rather than needing that column removed.
    #
    # The plaza is stamped at a 16px pitch with 32px tiles, so its stamps
    # overlap by half; PLAZA_RIGHT is the last fill column's origin, not the
    # plaza's right-hand pixel.
    rim = [emit(PLAZA_EDGE_R, 2, PLAZA_RIGHT, y)
           for y in range(PLAZA_TOP, PLAZA_BOTTOM + 1, 16)]

    out = ground + fence + rim + kept
    MAP.write_text("\n".join(out) + "\n")

    print(f"ground   : {dropped_ground} freehand stamps -> {len(ground)} on a "
          f"{cols}x{rows} grid")
    print(f"walls    : {dropped_wall} prop stamps removed")
    print(f"fence    : {len(fence)} added along the top and bottom edges")
    print(f"untouched: {len(kept)} tiles on layers 1+")
    print(f"total    : {len(lines)} -> {len(out)} lines")


if __name__ == "__main__":
    main()
