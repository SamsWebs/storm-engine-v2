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

# The cobbled patch is a 3x3 autotile read at a true 32px pitch: the whole ring
# and the centre all sit on src coordinates that are multiples of 32.
#
# Every entry must share that phase. An earlier version took the centre column
# from x=16 -- (16,32), (16,48), (16,96) -- which self-tiles perfectly (the
# interior art is 32px-periodic, so crop(16,48) == crop(48,48) exactly) and so
# looked right in isolation. It is not the same art as the phase-0 centre
# though: crop(16,48) and crop(32,64) differ in 464 of their 1024 pixels. Laid
# inside a phase-0 border ring the interior lattice lands half a diamond out,
# and every cell of the top row and both edge columns is cut into triangles.
#
# The tell is that it is only visible where interior meets border, so a
# composite of the interior alone -- or a hurried look at a small one -- passes.
STONE = {
    (-1, -1): (0, 32),  (0, -1): (32, 32),  (1, -1): (64, 32),
    (-1,  0): (0, 64),  (0,  0): (32, 64),  (1,  0): (64, 64),
    (-1,  1): (0, 96),  (0,  1): (32, 96),  (1,  1): (64, 96),
}

# Every source cell inside that patch. Anything drawn from here is stone and
# gets re-laid; everything else on layers 1+ is left where the artist put it.
STONE_SRCS = {(x, y) for x in (0, 16, 32, 48, 64)
              for y in (32, 48, 64, 80, 96, 112)}

# (srcX, srcY, axis, value) -- stamps of this tile on this line are the run.
# Identified by histogramming the map: each is a single prop repeated along one
# edge, which is what makes them read as a wall of barrels rather than scenery.
# The shop's roof stopped one column short of its own walls. Measured off the
# map: every roof row's rightmost tile sat at x=128, every facade row's at
# x=160, so the east end of the building stood uncovered.
#
# The two bottom-cap tiles that did sit at x=160 were not strays -- they were
# the one roof row that already reached the facade. Squaring the roof off by
# deleting them left it short; bringing the rest of the roof east to meet them
# is the actual fix. Each row's right cap moves to x=160, and x=128 becomes a
# middle tile.
ROOF_EAST_X = 160
ROOF_ROWS = {            # worldY: (right-cap src, middle src)
    64:  ((64, 0),  (32, 0)),      # ridge
    96:  ((64, 32), (32, 32)),     # body
    112: ((64, 32), (32, 32)),
    128: ((64, 32), (32, 32)),
    144: ((64, 32), (32, 32)),
    160: ((64, 32), (32, 32)),
    192: ((64, 64), (32, 64)),     # eave
}
ROOF_CAPS = {cap for cap, _ in ROOF_ROWS.values()}

# The facade carried no signage at all, on a building whose occupant opens with
# "step inside and browse my wares".
SHOP_SIGN = (0, 160)     # "STORE"; the sheet also has HEALTH, a cross, a padlock
SHOP_SIGN_AT = (64, 224)   # on the wall band, clear of the roof edge above

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


def emit(src, z, wx, wy, sheet="Outside-Tileset"):
    return (f"tiles {sheet} {TILE} {TILE} {src[0]} {src[1]} {z} "
            f"{wx} {wy} 1 1 0 0")


def main():
    lines = MAP.read_text().splitlines()
    shutil.copy(MAP, MAP.with_suffix(".map.bak"))

    kept, dropped_ground, dropped_wall, roof_extended = [], 0, 0, 0
    stone_cells = set()
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
        # Idempotence: drop the perimeter fence this script itself lays, so
        # re-running does not stack a second row on top of the first. Fence
        # elsewhere on the map is the artist's and is kept.
        if src == FENCE and wy in (0, LEVEL_H - TILE):
            continue
        if f[1] == "Buildings-Tileset":
            if src == SHOP_SIGN:
                continue          # re-emitted below, so re-runs do not stack
            # Any cap already at or past the east edge goes, whatever row it is
            # on. Gating this on `wy in ROOF_ROWS` left a duplicate eave cap at
            # (160,208) -- the map was painted at a 16px pitch, so a stamp can
            # sit half a row off the rows this script knows about, and that one
            # survived every regeneration.
            if src in ROOF_CAPS and wx >= ROOF_EAST_X:
                continue          # re-emitted at the proper rows below
            if src in ROOF_CAPS and wy in ROOF_ROWS:
                cap, mid = ROOF_ROWS[wy]
                if src != cap:
                    kept.append(line.rstrip())
                    continue
                # The old right cap becomes an ordinary middle tile; the cap
                # itself is laid at ROOF_EAST_X below.
                kept.append(emit(mid, z, wx, wy, "Buildings-Tileset"))
                roof_extended += 1
                continue
            kept.append(line.rstrip())
            continue
        if f[1] == "Outside-Tileset" and src in STONE_SRCS:
            # Mark every 32px cell this stamp touches, not just the one holding
            # its origin. The stamps are 32px wide but were placed every 16px,
            # so a stamp at an odd step straddles two cells; taking only the
            # origin's cell under-covers, and the region came out pocked with
            # holes and shed a detached corner. Rounding to the nearest cell has
            # the same fault. Over-covering can push a boundary out by at most
            # one cell, which is invisible on a solid region.
            for gx in {wx // TILE, (wx + TILE - 1) // TILE}:
                for gy in {wy // TILE, (wy + TILE - 1) // TILE}:
                    stone_cells.add((gx, gy))
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

    # Re-lay every stone region on the 32px grid, picking each cell's tile from
    # which of its four neighbours are also stone. The regions keep whatever
    # shape they were painted in -- this only fixes how they are drawn.
    stone = []
    for (cx, cy) in sorted(stone_cells):
        left = -1 if (cx - 1, cy) not in stone_cells else 0
        right = 1 if (cx + 1, cy) not in stone_cells else 0
        up = -1 if (cx, cy - 1) not in stone_cells else 0
        down = 1 if (cx, cy + 1) not in stone_cells else 0
        # A cell with stone on both sides is interior on that axis; one open
        # side picks that border, and both open (a one-cell-wide neck) falls
        # back to interior rather than drawing two borders in one tile.
        ex = left if right == 0 else (right if left == 0 else 0)
        ey = up if down == 0 else (down if up == 0 else 0)
        stone.append(emit(STONE[(ex, ey)], 1, cx * TILE, cy * TILE))

    # Roof's east column, bringing it out to the facade's own right edge.
    roof = [emit(cap, 2, ROOF_EAST_X, y, "Buildings-Tileset")
            for y, (cap, _) in sorted(ROOF_ROWS.items())]

    # Shopfront sign, on layer 3 so it sits over the facade it is fixed to.
    sign = [emit(SHOP_SIGN, 3, SHOP_SIGN_AT[0], SHOP_SIGN_AT[1],
                 "Buildings-Tileset")]

    out = ground + fence + stone + kept + roof + sign
    MAP.write_text("\n".join(out) + "\n")

    print(f"ground   : {dropped_ground} freehand stamps -> {len(ground)} on a "
          f"{cols}x{rows} grid")
    print(f"walls    : {dropped_wall} prop stamps removed")
    print(f"fence    : {len(fence)} added along the top and bottom edges")
    print(f"stone    : {len(stone_cells)} cells re-laid on the 32px grid")
    print(f"building : roof extended east ({roof_extended} caps -> middles, "
          f"{len(ROOF_ROWS)} laid at x={ROOF_EAST_X}), shop sign added")
    print(f"untouched: {len(kept)} tiles on layers 1+")
    print(f"total    : {len(lines)} -> {len(out)} lines")


if __name__ == "__main__":
    main()
