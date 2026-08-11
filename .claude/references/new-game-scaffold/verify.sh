#!/usr/bin/env bash
# Build a Storm! Engine v2 game, run it headless, and report whether it works.
#
# "It compiles" is not the bar, and neither is "it ran without complaining" --
# a game can do both while drawing an empty black window. This checks four
# things in order: something was actually written, it builds and links, it
# survives a run, and (when a display is available) it puts pixels on screen.
#
#   ./verify.sh              # build + run 4s
#   ./verify.sh 10           # build + run 10s
#   PRISTINE=/path ./verify.sh   # compare against a specific scaffold copy
#
# Exit 0 = pass. Non-zero = fail, with the reason on stderr.

set -uo pipefail

RUN_SECONDS="${1:-4}"
BIN_NAME="$(grep -m1 '^NAME' Makefile | sed 's/.*=[[:space:]]*//')"
BIN="bin/${BIN_NAME}"
LOG="$(mktemp)"
SHOT="$(mktemp --suffix=.png)"
trap 'rm -f "$LOG" "$SHOT"' EXIT

fail() { echo "FAIL: $*" >&2; exit 1; }

# ── 0. Did anyone actually write a game? ────────────────────────────────────
# An untouched scaffold builds and runs perfectly, so without this check a
# model that does nothing at all scores the same as one that succeeds. This
# has happened: two local models emitted no files and the old verifier said
# PASS.
PRISTINE="${PRISTINE:-$HOME/.claude/skills/storm-engine-ai-skills/references/new-game-scaffold}"
if [ -d "$PRISTINE" ]; then
    changed=0
    for f in src/states/playState.cpp src/states/playState.h src/game.cpp src/game.h src/main.cpp; do
        [ -f "$f" ] || continue
        [ -f "$PRISTINE/$f" ] || { changed=1; break; }
        cmp -s "$f" "$PRISTINE/$f" || { changed=1; break; }
    done
    # New source files also count as work.
    extra="$(find src -name '*.cpp' -o -name '*.h' 2>/dev/null | wc -l)"
    base="$(find "$PRISTINE/src" -name '*.cpp' -o -name '*.h' 2>/dev/null | wc -l)"
    [ "$extra" -ne "$base" ] && changed=1
    [ "$changed" -eq 1 ] || fail "no source file differs from the pristine scaffold -- nothing was written"
else
    echo "warn: no pristine scaffold at $PRISTINE; skipping the did-anything-change check" >&2
fi

# ── 1. Build ────────────────────────────────────────────────────────────────
echo "== build =="
make clean >/dev/null 2>&1
if ! make > "$LOG" 2>&1; then
    tail -20 "$LOG" >&2
    fail "build failed"
fi
[ -x "$BIN" ] || fail "build reported success but $BIN is missing"

# ── 2. Link ─────────────────────────────────────────────────────────────────
if ldd "$BIN" 2>/dev/null | grep -q "not found"; then
    ldd "$BIN" | grep "not found" >&2
    fail "unresolved shared libraries"
fi

# ── 3. Run headless ─────────────────────────────────────────────────────────
# offscreen supports a real renderer; dummy does not and fails
# SDL_RENDERER_ACCELERATED, which looks like a game bug but is not.
echo "== run (${RUN_SECONDS}s, offscreen) =="
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
    timeout "$RUN_SECONDS" stdbuf -oL -eL "./$BIN" > "$LOG" 2>&1
rc=$?

if [ "$rc" -ne 124 ]; then
    echo "--- output ---" >&2; tail -20 "$LOG" >&2
    case "$rc" in
        0)   fail "exited immediately (rc=0) -- Initialize() likely bailed before the loop" ;;
        139) fail "segfault (rc=139)" ;;
        134) fail "abort (rc=134) -- an ECS lookup on a missing tag/system, or -fno-exceptions" ;;
        *)   fail "exited early with rc=$rc" ;;
    esac
fi

if grep -q "ERR" "$LOG"; then
    grep "ERR" "$LOG" >&2
    fail "engine logged an error"
fi

textures=$(grep -c "New texture added" "$LOG")
entities=$(grep -c "Entity created" "$LOG")
[ "$textures" -gt 0 ] || fail "no textures loaded -- check asset paths are relative to the run directory"
[ "$entities" -gt 0 ] || fail "no entities created -- onEnter() may not have run"

# ── 4. Does it actually draw anything? ──────────────────────────────────────
# Everything above passes on a completely black window: textures load, entities
# exist, no errors. A sprite whose srcRect falls outside its texture, or which
# samples a transparent cell, renders nothing and says nothing. This is the
# only check that catches it.
draw="skipped (no DISPLAY)"
if [ -n "${DISPLAY:-}" ] && command -v import >/dev/null && command -v xprop >/dev/null; then
    echo "== draw check (windowed, ${DISPLAY}) =="
    SDL_AUDIODRIVER=dummy timeout $((RUN_SECONDS + 4)) "./$BIN" >/dev/null 2>&1 &
    game_pid=$!
    sleep 3

    # Match the window by PID, never by title. Matching on a title substring
    # picks up any window on the desktop that happens to contain the word --
    # an editor with the engine repo open, a terminal in the project directory
    # -- and then measures ITS pixels. That produced a confident pass on a game
    # rendering pure black.
    # `timeout` execs the game as a child, so the pid that owns the window is
    # usually one level down.
    real_pid="$(ps -o pid= --ppid "$game_pid" 2>/dev/null | tr -d ' ' | head -1)"
    real_pid="${real_pid:-$game_pid}"

    # Walk the whole tree, not just root's children: the window manager
    # reparents SDL windows, so root's direct children are WM frames that carry
    # no _NET_WM_PID. The client window is nested inside one of them.
    win=""
    for wid in $(xwininfo -root -tree 2>/dev/null | grep -oE '0x[0-9a-f]+' | sort -u); do
        wpid="$(xprop -id "$wid" _NET_WM_PID 2>/dev/null | grep -oE '[0-9]+$')"
        [ -n "$wpid" ] || continue
        if [ "$wpid" = "$real_pid" ] || [ "$wpid" = "$game_pid" ]; then
            win="$wid"; break
        fi
    done

    # Raise the window before grabbing it. Without a compositor, `import
    # -window` on an occluded window returns whatever is physically on screen
    # in that region -- which reads as a blank game and produced a false
    # "renders nothing" verdict on a working one. Sample a few times and keep
    # the best frame, since a single grab can also land mid-clear.
    [ -n "$win" ] && command -v wmctrl >/dev/null && wmctrl -i -a "$win" 2>/dev/null
    sleep 1

    best=0
    if [ -n "$win" ]; then
        for _ in 1 2 3; do
            import -window "$win" "$SHOT" 2>/dev/null || continue
            n="$(python3 - "$SHOT" <<'PYEOF'
import sys, collections
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
px = list(im.getdata())
print(0 if not px else len(px) - collections.Counter(px).most_common(1)[0][1])
PYEOF
)"
            [ "${n:-0}" -gt "$best" ] && best="$n"
            sleep 0.5
        done
    fi

    if [ -n "$win" ]; then
        npx="$best"
        kill "$game_pid" 2>/dev/null; wait "$game_pid" 2>/dev/null
        if [ "$npx" -lt 100 ]; then
            fail "window is blank (${npx} non-background pixels) -- sprites are not drawing. Check that every SpriteComponent's width/height match its sheet cell (they define srcRect, so a 16x90 paddle from a 32x32 cell samples off the texture) and that srcRectX is not a transparent cell"
        fi
        draw="drew content (${npx} non-background px)"
    else
        kill "$game_pid" 2>/dev/null; wait "$game_pid" 2>/dev/null
        draw="skipped (could not locate the game window by pid)"
    fi
fi

echo "PASS: built, linked, ran ${RUN_SECONDS}s, ${textures} texture(s), ${entities} entity/entities, no errors; ${draw}"
