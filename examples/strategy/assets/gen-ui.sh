#!/usr/bin/env bash
#
# Regenerates assets/ui/ -- the digit strip and the text labels.
#
# The engine has no text rendering (common/ carries five components and five
# systems, and nothing links SDL_ttf), so every string in this game is a
# pre-rendered PNG. Tiny Swords ships buttons, banners and icons but no font,
# so these are the one set of images this example has to supply itself.
#
# They are generated rather than sourced on purpose: that keeps them free of
# any third-party licence, and it means a change of wording is a one-line edit
# here instead of a hand-drawn asset.
#
# Requires ImageMagick and DejaVu Sans Bold.

set -euo pipefail

cd "$(dirname "$0")"
OUT=ui
FONT=/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf

if ! command -v convert >/dev/null; then
    echo "gen-ui.sh: ImageMagick's 'convert' is not on PATH" >&2
    exit 1
fi
if [ ! -f "$FONT" ]; then
    echo "gen-ui.sh: $FONT not found; point FONT at any bold TTF" >&2
    exit 1
fi

mkdir -p "$OUT"

# Palette, picked to sit inside Tiny Swords' own range rather than beside it:
# the pack outlines everything in a near-black plum and fills its UI with warm
# parchment. Pure black on the green field reads as a hole in the screen.
OUTLINE='#2b1e3d'   # deep plum, the pack's outline colour
PARCHMENT='#f6e7c8' # default text
GOLD='#ffd75e'      # a win
BLOOD='#ff9a9a'     # a loss
DIM='#cbb894'       # hint lines, quieter than the rest

# Draws TEXT twice and composites: a thick OUTLINE pass underneath, then the
# fill on top. The obvious one-pass form
#     -stroke black -strokewidth N label:TEXT -fill C -annotate 0 TEXT
# does NOT work -- the stroked pass is laid down first and the -annotate fill
# lands under it, so every label came out effectively solid black with a hairline
# of colour at the edges.
#
# outlined <outfile> <text> <pointsize> <fill>
outlined() {
    local file=$1 text=$2 size=$3 fill=$4
    convert \
        \( -background none -font "$FONT" -pointsize "$size" \
           -fill "$OUTLINE" -stroke "$OUTLINE" -strokewidth 6 label:"$text" \) \
        \( -background none -font "$FONT" -pointsize "$size" \
           -fill "$fill" -stroke none label:"$text" \) \
        -gravity center -composite -trim +repage "$OUT/$file"
}

# Digit cell size. ui.h does its arithmetic from these two numbers, so they are
# duplicated in src/ui.h as DIGIT_W / DIGIT_H -- change both together.
DW=24
DH=32

# Each digit is rendered into its own fixed-size cell and the cells are then
# appended. Rendering "0123456789" as one string would space the glyphs by the
# font's own advance widths, which differ per digit ('1' is much narrower than
# '0'), and the uniform-pitch arithmetic in ui.h would drift across the number.
rm -f "$OUT"/_d?.png
for d in 0 1 2 3 4 5 6 7 8 9; do
    outlined "_d$d.png" "$d" 30 "$PARCHMENT"
    # Re-pad to the fixed cell: `outlined` trims to the ink, and the trimmed
    # width differs per digit, which is exactly the drift the fixed pitch exists
    # to avoid.
    convert "$OUT/_d$d.png" -background none -gravity center \
            -extent ${DW}x${DH} "$OUT/_d$d.png"
done
convert "$OUT"/_d0.png "$OUT"/_d1.png "$OUT"/_d2.png "$OUT"/_d3.png "$OUT"/_d4.png \
        "$OUT"/_d5.png "$OUT"/_d6.png "$OUT"/_d7.png "$OUT"/_d8.png "$OUT"/_d9.png \
        +append "$OUT/digits.png"
rm -f "$OUT"/_d?.png

outlined title.png        "REALMS"       96 "$GOLD"
outlined menu_start.png   "START"        48 "$PARCHMENT"
outlined menu_quit.png    "QUIT"         48 "$PARCHMENT"
outlined day.png          "DAY"          28 "$PARCHMENT"
outlined cmd_charge.png   "CHARGE"       32 "$PARCHMENT"
outlined cmd_hold.png     "HOLD"         32 "$PARCHMENT"
outlined cmd_volley.png   "VOLLEY"       32 "$PARCHMENT"
outlined cmd_retreat.png  "RETREAT"      32 "$PARCHMENT"
outlined victory.png      "VICTORY"      64 "$GOLD"
outlined defeat.png       "DEFEAT"       64 "$BLOOD"
outlined win.png          "THE REALM IS YOURS" 56 "$GOLD"
outlined lose.png         "THE REALM IS LOST"  56 "$BLOOD"
outlined hint_map.png     "ARROWS SELECT   ENTER MARCH   ESC QUIT"   20 "$DIM"
outlined hint_battle.png  "1 CHARGE   2 HOLD   3 VOLLEY   4 RETREAT" 20 "$DIM"

echo "gen-ui.sh: wrote $(ls -1 "$OUT" | wc -l) files to $OUT/"
