#!/usr/bin/env bash
# Score a directory of generated games.
#
#   ./run-eval.sh <candidates-dir> [run-seconds]
#
# Expects one subdirectory per candidate, each a standalone game with a
# Makefile and verify.sh (i.e. copied from references/new-game-scaffold/).
# Prints a per-candidate level 0-3 and a summary. Level 4 is per-task and is
# not scored here -- see each task's assertions.

set -uo pipefail

DIR="${1:?usage: run-eval.sh <candidates-dir> [run-seconds]}"
SECS="${2:-4}"
[ -d "$DIR" ] || { echo "no such directory: $DIR" >&2; exit 2; }

pass=0; total=0
printf "%-24s %-7s %s\n" "CANDIDATE" "LEVEL" "FIRST ERROR"
printf "%-24s %-7s %s\n" "------------------------" "-------" "-----------"

for c in "$DIR"/*/; do
    [ -d "$c" ] || continue
    name="$(basename "$c")"
    total=$((total + 1))

    if [ ! -f "$c/verify.sh" ]; then
        printf "%-24s %-7s %s\n" "$name" "0" "no verify.sh (not scaffold-derived)"
        continue
    fi

    out="$( cd "$c" && chmod +x verify.sh 2>/dev/null; cd "$c" && ./verify.sh "$SECS" 2>&1 )"
    rc=$?

    if [ $rc -eq 0 ]; then
        level=3
        first="-"
        pass=$((pass + 1))
    else
        # Map the verifier's failure to a level.
        first="$(grep -m1 '^FAIL:' <<<"$out" | sed 's/^FAIL: //')"
        case "$first" in
            "build failed"*)                 level=0 ;;
            "unresolved shared libraries"*)  level=1 ;;
            *)                               level=2 ;;
        esac
        [ -n "$first" ] || first="$(tail -1 <<<"$out")"
    fi

    printf "%-24s %-7s %s\n" "$name" "$level" "$first"
done

echo
echo "level 3: $pass / $total"
[ "$total" -gt 0 ] && [ "$pass" -eq "$total" ]
