#!/usr/bin/env bash
#
# Regenerates assets/ui/ -- the digit strip, the text labels and the window icon.
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
FONT=${FONT:-/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf}

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

# Retreat confirmation. Retreat is the one order that cannot be taken back -- it
# concedes the castle the instant it lands -- so it asks first.
outlined confirm_retreat.png "RETREAT?"                            56 "$BLOOD"
outlined confirm_body.png    "THE CASTLE IS LOST"                  26 "$PARCHMENT"
outlined confirm_hint.png    "ENTER / A  CONFIRM      ESC / B  CANCEL" 20 "$DIM"

# ---------------------------------------------------------------------------
# Window icon.
#
# Drawn from primitives rather than cut from the artwork, which is what
# examples/shooter does with its sprite sheet. That is not an option here: Tiny
# Swords may not be redistributed, so anything derived from it would have to be
# gitignored along with the rest of gfx/ and the repository would ship a game
# with no icon. Generating it keeps the icon committed, licence-free, and
# available before the player has downloaded anything.
#
# +antialias (i.e. antialiasing OFF) throughout: this sits next to pixel art,
# and smoothed edges read as blurry at 32px rather than smooth.
OL='#2b1e3d'    # outline, same plum as the text
ST='#f6e7c8'    # stone
SH='#cdb492'    # stone course
GOLD='#ffd75e'
BL='#5a8ce6'    # the Blue faction's pennant
RD='#d25050'    # the Red faction's

convert -size 64x64 xc:none +antialias \
    `# pennants, one per faction, flown from the tower tops` \
    -fill "$OL" -draw "rectangle 13,4 14,18" \
    -fill "$BL" -draw "polygon 15,5 26,9 15,13" \
    -fill "$OL" -draw "rectangle 49,4 50,18" \
    -fill "$RD" -draw "polygon 48,5 37,9 48,13" \
    `# curtain wall, set lower than the towers so the silhouette has a step` \
    -fill "$OL" -draw "rectangle 20,28 44,58" \
    -fill "$ST" -draw "rectangle 22,30 42,56" \
    -fill "$OL" -draw "rectangle 22,28 26,33" -draw "rectangle 30,28 34,33" \
                -draw "rectangle 38,28 42,33" \
    `# towers` \
    -fill "$OL" -draw "rectangle 6,16 22,58"  -fill "$ST" -draw "rectangle 8,18 20,56" \
    -fill "$OL" -draw "rectangle 42,16 58,58" -fill "$ST" -draw "rectangle 44,18 56,56" \
    -fill "$OL" -draw "rectangle 8,16 11,21"  -draw "rectangle 15,16 18,21" \
                -draw "rectangle 44,16 47,21" -draw "rectangle 51,16 54,21" \
    -fill "$OL" -draw "rectangle 12,26 15,32" -draw "rectangle 48,26 51,32" \
    `# stone course on the towers only -- run across the whole front it cuts
     # through the gate arch and the shape stops reading` \
    -fill "$SH" -draw "rectangle 8,40 20,42" -draw "rectangle 44,40 56,42" \
    `# gate` \
    -fill "$OL"      -draw "roundrectangle 26,38 38,58 7,7" \
    -fill "#4a3a5e"  -draw "roundrectangle 28,40 36,58 6,6" \
    -fill "$GOLD"    -draw "rectangle 31,47 33,49" \
    "$OUT/icon.png"

echo "gen-ui.sh: wrote $(ls -1 "$OUT" | wc -l) files to $OUT/"
