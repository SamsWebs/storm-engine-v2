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
    convert -size ${DW}x${DH} xc:none \
        -font "$FONT" -pointsize 30 \
        -stroke black -strokewidth 3 -gravity center -annotate 0 "$d" \
        -stroke none -fill white -gravity center -annotate 0 "$d" \
        "$OUT/_d$d.png"
done
convert "$OUT"/_d0.png "$OUT"/_d1.png "$OUT"/_d2.png "$OUT"/_d3.png "$OUT"/_d4.png \
        "$OUT"/_d5.png "$OUT"/_d6.png "$OUT"/_d7.png "$OUT"/_d8.png "$OUT"/_d9.png \
        +append "$OUT/digits.png"
rm -f "$OUT"/_d?.png

# label <file> <text> <pointsize> [fill]
label() {
    local file=$1 text=$2 size=$3 fill=${4:-white}
    convert -background none \
        -font "$FONT" -pointsize "$size" \
        -stroke black -strokewidth 3 label:"$text" \
        -stroke none -fill "$fill" -annotate 0 "$text" \
        -trim +repage "$OUT/$file"
}

label title.png        "REALMS"        96
label menu_start.png   "START"         48
label menu_quit.png    "QUIT"          48
label day.png          "DAY"           28
label cmd_charge.png   "CHARGE"        32
label cmd_hold.png     "HOLD"          32
label cmd_volley.png   "VOLLEY"        32
label cmd_retreat.png  "RETREAT"       32
label victory.png      "VICTORY"       64 "#ffd75e"
label defeat.png       "DEFEAT"        64 "#ff6b6b"
label win.png          "THE REALM IS YOURS"  56 "#ffd75e"
label lose.png         "THE REALM IS LOST"   56 "#ff6b6b"
label hint_map.png     "ARROWS SELECT   ENTER MARCH   ESC QUIT"   20
label hint_battle.png  "1 CHARGE   2 HOLD   3 VOLLEY   4 RETREAT" 20

echo "gen-ui.sh: wrote $(ls -1 "$OUT" | wc -l) files to $OUT/"
