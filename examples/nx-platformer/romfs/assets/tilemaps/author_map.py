#!/usr/bin/env python3
"""Author platformer.map: terrain from a heightmap, everything else overlaid.

The surface is a heightmap so grass can only ever sit on top of its own dirt
column -- hand-drawn ASCII let grass run underneath the plateaus. Decorations,
floating platforms and props are placed by (col,row) on top.
"""
import sys
from PIL import Image

TS = 'examples/platformer/assets/tilemaps/16x16-platformer.png'
ASSET, T, COLS, ROWS = '16x16-platformer', 16, 40, 28

TILE = {  # name -> (col,row) in the tileset
    'grass_l': (0, 0), 'grass_m': (1, 0), 'grass_r': (2, 0),
    'dirt':    (1, 1), 'dirt_d':  (1, 2),
    'plat_l':  (3, 0), 'plat_m':  (4, 0), 'plat_r':  (6, 0),
    'ladder':  (0, 3), 'bush': (1, 3), 'rock': (1, 4), 'rock_s': (1, 5),
    'stump':   (2, 5), 'cloud': (0, 6), 'bee': (4, 7), 'turtle': (6, 7),
    'blk_q':   (7, 3), 'blk_y': (7, 2), 'pebble': (3, 2),
}

# Surface row per column. Lower number = higher ground.
H = ([22]*10 + [25]*10 + [19]*6 + [25]*4 + [17]*6 + [25]*4)
assert len(H) == COLS, len(H)

# (col, row, tile, solid, z)
PROPS = [
    # clouds, drifting behind everything
    (3, 1, 'cloud', False, -1), (18, 2, 'cloud', False, -1), (34, 1, 'cloud', False, -1),
    # floating platforms: (startCol, row, length)
]
PLATFORMS = [(7, 16, 3), (12, 11, 3), (18, 13, 3), (24, 8, 3), (31, 12, 3)]
BLOCKS = [(13, 8, 'blk_q'), (15, 8, 'blk_q'), (25, 5, 'blk_y'), (27, 5, 'blk_y')]
CRITTERS = [(26, 3, 'bee'), (20, 18, 'turtle')]

def build():
    tiles = []
    def put(col, row, name, solid, z=0):
        tc, tr = TILE[name]
        tiles.append(dict(srcX=tc*T, srcY=tr*T, z=z, wx=col*T, wy=row*T, solid=solid))

    # terrain: grass cap, then dirt all the way down
    for col in range(COLS):
        top = H[col]
        left  = (col == 0)          or H[col-1] != top
        right = (col == COLS-1)     or H[col+1] != top
        cap = 'grass_l' if left and not right else 'grass_r' if right and not left else 'grass_m'
        put(col, top, cap, True)
        for row in range(top+1, ROWS):
            put(col, row, 'dirt' if row < top+3 else 'dirt_d', True)

    for start, row, length in PLATFORMS:
        for i in range(length):
            name = 'plat_l' if i == 0 else 'plat_r' if i == length-1 else 'plat_m'
            put(start+i, row, name, True)

    for col, row, name in BLOCKS:
        put(col, row, name, True)
    for col, row, name in CRITTERS:
        put(col, row, name, False, 1)
    for col, row, name, solid, z in PROPS:
        put(col, row, name, solid, z)

    # ladders: run from the lip of a raised section down to the ground beside it
    for col, hi, lo in [(20, 19, 25), (30, 17, 25)]:
        for row in range(hi, lo):
            put(col-1, row, 'ladder', False, 1)

    # ground decoration, sat on the surface of its own column
    for col, name in [(3,'bush'), (5,'rock'), (7,'stump'), (13,'pebble'),
                      (16,'bush'), (17,'rock_s'), (36,'bush'), (38,'stump')]:
        put(col, H[col]-1, name, False, 1)
    return tiles

def write_map(tiles, path):
    with open(path, 'w') as f:
        for t in tiles:
            f.write(f"tiles {ASSET} {T} {T} {t['srcX']} {t['srcY']} {t['z']} "
                    f"{t['wx']} {t['wy']} 1 1 ")
            f.write(f"1 {T} {T} 0 0 0 \n" if t['solid'] else "0 0 \n")

def preview(tiles, path, zoom=2):
    ts = Image.open(TS).convert('RGBA')
    img = Image.new('RGBA', (COLS*T, ROWS*T), (99, 155, 255, 255))
    for t in sorted(tiles, key=lambda t: t['z']):
        img.alpha_composite(ts.crop((t['srcX'], t['srcY'], t['srcX']+T, t['srcY']+T)),
                            (t['wx'], t['wy']))
    img.resize((COLS*T*zoom, ROWS*T*zoom), Image.NEAREST).save(path)

if __name__ == '__main__':
    tiles = build()
    write_map(tiles, sys.argv[1]); preview(tiles, sys.argv[2])
    s = sum(1 for t in tiles if t['solid'])
    print(f"{len(tiles)} tiles ({s} solid, {len(tiles)-s} decorative)")
