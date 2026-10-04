#!/usr/bin/env python3
"""Drive a built example under a private display and report, per key, whether
the screen actually changed.

Track 0.6 of docs/ROADMAP.md: the engine ships a dozen-plus examples with UI
and no way to LOOK at one without playing to it. The rule this enforces is
"look at a screen you changed" -- which, in the game that first adopted a
layout harness, caught four layout defects in one session.

    python3 tools/screen-sweep.py examples/platformer
    python3 tools/screen-sweep.py examples/platformer --keys Escape,d,a,Left,Right,space
    python3 tools/screen-sweep.py --all

WHY ONE LAUNCH PER KEY

A keypress that opens a screen leaves you on the NEXT screen, so every later
key in the sweep is then measuring a different one. Pressing Escape on a menu
that quits ends the run entirely. One fresh process per key is the only way the
table means what it says. It costs a second per key; correctness is worth more.

WHY "CHANGED PIXELS" AND NOT A SCREENSHOT

The failure this exists to catch is a screen whose input path is DEAD while
every other check still passes. A spec asserts on state, a build asserts on
compilation, and neither of them ever presses a key. So the signal is a
byte-level comparison of the frame before and after the keypress:

    changed == 0   the key did NOTHING a player could see
    changed >  0   something moved -- which is necessary, not sufficient

A large count usually means the camera scrolled or the screen changed, not that
the thing under test moved, so read the kept PNG pair before believing it. A
count that exactly matches another key's is a finding, not a coincidence: two
keys driving one action is a duplicate binding.

THREE THINGS THIS GETS RIGHT THAT A NAIVE SCRIPT DOES NOT

  * `pkill -f <name>` also matches the shell running this script, because the
    repo path in the command line contains the binary name. Everything here
    kills by exact PID, and stale instances are cleared with `pkill -x`.

  * The binary name is READ FROM THE EXAMPLE'S MAKEFILE, never guessed from the
    directory. Guessing is wrong for two examples in this repo right now:
    shooter is `1945`, not `alienattack`, and strategy is `realms`, not `tanks`
    -- the names docs/CLAUDE.md used to print. A stale bin/ is rebuilt rather
    than trusted, for the same reason.

  * The window is found by PID (`xdotool search --pid`), not by a title string
    copied out of the source. Titles differ per example and one of them
    ("Realms") is also the binary name, which is the kind of coincidence that
    makes a title search match the wrong window.

WHAT THE NUMBERS ARE, AND WHAT THEY ARE NOT

Read DEAD first -- a byte-identical frame pair after a keypress is the strongest
signal here, and it is invisible to the spec suite, which asserts on state and
never presses a key.

Then read the BOX, and do not read the count as a verdict. Two measured examples
from building this:

  * examples/puzzle draws its PAUSED overlay in 1,039 px against a 6,144 px
    idle floor. Any threshold calls a pause that visibly works DEAD. The box
    (97x19+711+636) is a small static strip, which is what an overlay looks
    like; the unhandled keys Return and q change regions INSIDE the animating
    playfield and NEXT preview, which is what churn looks like. Same count
    range, opposite conclusions -- so the verdict says BELOW-FLOOR and means
    "look", not "broken".

  * A single after-shot misses a jump that starts and lands inside the settle
    window. The platformer's jump was reported dead until the tool started
    sampling several frames and keeping the maximum. The screenshot showed the
    player standing on the ground in both frames, having already come down.

So: the tool's job is to make LOOKING cheap and to catch byte-identical frames
automatically. It is not entitled to conclude that a key is broken.

Requires Xvfb, xdotool and ImageMagick (import/compare/convert). Stdlib-only
otherwise. Not in CI: the image has no Xvfb, and every example needs an
installed engine, so this is a local tool by construction.
"""
import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent

# Keys worth sweeping where an example has no manifest of its own. Chosen to
# include the letters a player reaches for, not just the arrows: a letter that
# is also an action is the single largest source of "the screen jumped
# somewhere I didn't ask for".
DEFAULT_KEYS = ["Escape", "Return", "space", "Up", "Down", "Left", "Right",
                "d", "a", "w", "s", "1", "2", "3"]

# Keys that need a HOLD to be visible. A tapped key that sets a flag and clears
# it on KEYUP can legitimately leave the frame identical; a held one cannot.
HOLD_KEYS = {"Left", "Right", "Up", "Down", "a", "d", "w", "s", "space"}


class ToolMissing(Exception):
    pass


def need(*tools):
    missing = [t for t in tools if not shutil.which(t)]
    if missing:
        raise ToolMissing("not found on PATH: " + ", ".join(missing))


def run(argv, env=None, timeout=20):
    """subprocess.run with a timeout, and never raising on a non-zero exit.

    Every external tool here goes through this. `xdotool windowactivate` and
    `xdotool key` both block indefinitely against a display with no window
    manager, and a sweep that hangs with no output and no child process costs
    more to diagnose than the check is worth.
    """
    try:
        return subprocess.run(argv, env=env, capture_output=True, text=True,
                              timeout=timeout)
    except subprocess.TimeoutExpired as e:
        return subprocess.CompletedProcess(
            argv, 124, e.stdout or "", (e.stderr or "") + "\n[timed out]")


# ── the example under test ────────────────────────────────────────────────────
def read_name(exdir: pathlib.Path) -> str:
    """The binary's name, from the example's own Makefile.

    Deliberately not derived from the directory name: shooter/ builds `1945`
    and strategy/ builds `realms`. Guessing here fails on exactly the two
    examples whose names a reader would most confidently get wrong.
    """
    mk = exdir / "Makefile"
    if not mk.is_file():
        raise SystemExit(f"{exdir}: no Makefile, so no NAME to run")
    m = re.search(r"^\s*NAME\s*:?=\s*(\S+)", mk.read_text(), re.MULTILINE)
    if not m:
        raise SystemExit(f"{exdir}/Makefile has no NAME = ... line")
    return m.group(1)


def ensure_built(exdir: pathlib.Path, name: str) -> pathlib.Path:
    """Run make, then prove the binary actually LOADS.

    Two traps, both hit while building this:

    * An mtime comparison is not a freshness test. examples/platformer/bin held
      a binary that was newer than every source and still could not run here --
      it wanted GLIBC_2.34 and GLIBCXX_3.4.29 from the machine it was built on.
      A sweep over it reported "no window appeared" for every key, which reads
      like a dead input path and is nothing of the kind. So `make` is invoked
      unconditionally: base.mk's -MMD tracking makes that a no-op when the tree
      is current, and it relinks when it is not.

    * examples/strategy/bin/tanks sat next to a Makefile whose NAME had become
      `realms`. A bin/ directory is a cache, not a fact.
    """
    # PWD, not just cwd. examples/examples.mk derives BIN_DIR from $(PWD), and
    # `subprocess(cwd=...)` does NOT update the PWD environment variable -- it
    # behaves exactly like `make -C` here, which the CI script's own header
    # warns about. The first version of this tool ran make with cwd= and the
    # binary landed in the REPO ROOT's bin/ instead of the example's, which then
    # read as "make succeeded but the binary is absent". So PWD is set to match.
    build_env = {**os.environ, "PWD": str(exdir)}
    r = subprocess.run(["make"], cwd=exdir, env=build_env,
                       capture_output=True, text=True, timeout=900)
    if r.returncode != 0:
        raise SystemExit(f"{exdir}: make failed\n{r.stdout[-2000:]}\n"
                         f"{r.stderr[-2000:]}")
    binary = exdir / "bin" / name
    if not binary.is_file():
        found = sorted(p.name for p in (exdir / "bin").glob("*")) \
            if (exdir / "bin").is_dir() else []
        raise SystemExit(f"{exdir}: make succeeded but bin/{name} is absent. "
                         f"bin/ holds: {found or 'nothing'}")

    # Loader check, cheaper and clearer than a 12s window timeout: start it,
    # give it a moment, and report the dynamic-linker complaint verbatim.
    probe_env = {**os.environ, "DISPLAY": os.environ.get("DISPLAY", ":99")}
    p = subprocess.Popen([str(binary)], cwd=str(exdir), env=probe_env,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True)
    try:
        out, _ = p.communicate(timeout=4)
        if p.returncode is not None and p.returncode != 0:
            tail = "\n".join((out or "").strip().splitlines()[:6])
            raise SystemExit(
                f"{exdir}: {name} exits immediately (rc={p.returncode}). "
                f"It is not a usable artifact:\n{tail}\n"
                "Rebuild it on this machine before sweeping.")
    except subprocess.TimeoutExpired:
        p.terminate()          # it was still alive at 4s: it launches
    return binary


# ── display ───────────────────────────────────────────────────────────────────
class Display:
    """A private X server for the run, so the sweep never touches the desktop.

    Started with setsid: without it, a Ctrl-C on this script takes the X server
    with it and every later run fails to open a display.
    """

    def __init__(self, num: int = 99):
        self.num = num
        self.proc = None
        self.ours = False

    def __enter__(self):
        lock = pathlib.Path(f"/tmp/.X{self.num}-lock")
        if lock.exists():
            return self  # someone (a previous run) already has it up
        subprocess.run(["Xvfb", f":{self.num}", "-screen", "0", "1280x720x24"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       start_new_session=True)
        for _ in range(50):
            if lock.exists():
                break
            time.sleep(0.1)
        self.ours = True
        return self

    def __exit__(self, *exc):
        if self.ours and self.proc:
            self.proc.terminate()
        return False

    @property
    def env(self):
        return {**os.environ, "DISPLAY": f":{self.num}"}


def find_window(env, pid: int, timeout=12.0):
    """The window belonging to THIS pid. Never a title match."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        r = run(["xdotool", "search", "--pid", str(pid)], env)
        ids = [ln for ln in r.stdout.split() if ln.strip()]
        if ids:
            wid = ids[-1]
            # No --sync here. --sync blocks until the window is ACTUATED, which
            # needs a window manager; a bare Xvfb has none, so the first
            # version of this script deadlocked on this line with no child
            # process and no output, which is a miserable thing to debug.
            # key/import work fine against an unfocused window.
            run(["xdotool", "windowfocus", wid], env, timeout=5)
            # Let the first frame land before capturing.
            time.sleep(1.2)
            return wid
        time.sleep(0.25)
    return None


def shoot(env, wid, path: pathlib.Path):
    r = run(["import", "-window", wid, str(path)], env)
    if r.returncode != 0:
        raise SystemExit(f"import failed: {r.stderr}")


def diff_region(a: pathlib.Path, b: pathlib.Path):
    """(changed_pixels, bbox) for a frame pair.

    The bbox is not decoration. A global pixel COUNT cannot tell a small
    overlay from animation churn, and getting that wrong hides working
    features: examples/puzzle draws its PAUSED overlay in ~1,000 px against a
    ~6,100 px idle floor, so any threshold calls a pause that visibly works
    dead. The bbox says WHERE the change was -- a 150x30 strip is an overlay, a
    region the size of the window is a camera move, and neither is a number
    that can be compared against a threshold.

    Returns bbox None when the frames are identical.
    """
    r = run(["compare", "-metric", "AE", str(a), str(b), "null:"])
    count = -1
    for line in (r.stderr or r.stdout).strip().splitlines():
        line = line.strip()
        if line.isdigit():
            count = int(line)
            break
    if count == 0:
        return 0, None
    # %X and %Y already carry their own sign, so the format string must not
    # add one: "w%X" emits "97x19+711+636", while "%w+%X+%Y" emits
    # "97x19++711++636" and the regex below rejects it -- which is how the bbox
    # silently came back None on the first run.
    t = run(["convert", str(a), str(b), "-compose", "difference",
             "-composite", "-trim", "-format", "%wx%h%X%Y", "info:"])
    box = (t.stdout or "").strip()
    if not re.fullmatch(r"\d+x\d+\+\d+\+\d+", box):
        box = None
    return count, box


def changed_pixels(a: pathlib.Path, b: pathlib.Path) -> int:
    return diff_region(a, b)[0]


# ── one key, one process ──────────────────────────────────────────────────────
def probe(binary: pathlib.Path, exdir: pathlib.Path, env, key, outdir: pathlib.Path,
          settle: float, baseline: int, samples: int = 5, gap: float = 0.18):
    """Launch, capture, press, SAMPLE, diff, kill. Always kills by pid.

    Sampling, not a single after-shot. A jump that starts and lands inside the
    settle window leaves the final frame identical to the first, and the first
    version of this sweep reported the platformer's jump as dead for exactly
    that reason -- the screenshot plainly showed the player on the ground in
    both frames, having already come down. The MAXIMUM delta across a short
    burst of samples catches transient motion, and a screen transition, and a
    one-frame flash, none of which survive a single after-shot.

    `baseline` is the animation drift measured with NO key pressed at all. It is
    used in the verdict, not in the number: the raw pixel count is kept because
    it is the evidence a reader checks, and because two keys reading the same
    count is itself a finding.
    """
    outdir.mkdir(parents=True, exist_ok=True)
    safe = re.sub(r"[^A-Za-z0-9]", "_", key)
    before = outdir / f"{safe}-before.png"
    peak = outdir / f"{safe}-after.png"   # the frame that produced the max

    proc = subprocess.Popen([str(binary)], cwd=str(exdir), env=env,
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    try:
        wid = find_window(env, proc.pid)
        if wid is None:
            return dict(key=key, changed=None, box=None,
                        note="no window appeared", before=None, after=None,
                        exited=False)
        shoot(env, wid, before)

        if key in HOLD_KEYS:
            # Movement needs a real HOLD, not a tap: at 0.7s the platformer
            # player travelled less than one idle animation frame swap.
            run(["xdotool", "keydown", key], env)
            time.sleep(max(settle, 1.5))
            run(["xdotool", "keyup", key], env)
        else:
            run(["xdotool", "key", key], env)

        best, best_i, best_box, shots = -1, -1, None, []
        for i in range(samples):
            time.sleep(gap if i else max(0.08, gap / 2))
            if proc.poll() is not None:
                break
            # Each sample gets its own file: a single reused temp path cannot
            # name the frame that produced the max, and shutil then refuses to
            # copy it onto itself.
            shot = outdir / f".{safe}-s{i}.png"
            shoot(env, wid, shot)
            shots.append(shot)
            n, box = diff_region(before, shot)
            if n > best:
                best, best_i, best_box = n, i, box
        if best_i >= 0:
            shutil.copyfile(shots[best_i], peak)
        for s in shots:
            if s.exists():
                s.unlink()

        note = ""
        # A key that ends the run (Escape on most examples) leaves no frames to
        # compare. That is a real, reportable outcome -- "you cannot sweep past
        # this key" -- and NOT the same claim as "this key did nothing".
        exited = proc.poll() is not None
        if exited:
            note = f"process exited (rc={proc.returncode})"
        return dict(key=key, changed=best, box=best_box, note=note,
                    before=str(before),
                    after=str(peak) if best >= 0 else None, exited=exited)
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()


def measure_baseline(binary, exdir, env, outdir, settle) -> int:
    """Pixels that change with NOTHING pressed.

    Without this, every key reads as live. The first run of this sweep over
    examples/platformer reported 1,093 px for `q`, a key the example does not
    handle at all -- the idle animation cycle, not input. A tool that cannot
    tell a dead key from a breathing sprite reports "everything works" for a
    screen whose input path never ran, which is the exact failure this whole
    exercise exists to catch.
    """
    outdir.mkdir(parents=True, exist_ok=True)
    a, b = outdir / "_idle-a.png", outdir / "_idle-b.png"
    proc = subprocess.Popen([str(binary)], cwd=str(exdir), env=env,
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    try:
        wid = find_window(env, proc.pid)
        if wid is None:
            return -1
        shoot(env, wid, a)
        time.sleep(settle)
        shoot(env, wid, b)
        return changed_pixels(a, b)
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()


def verdict(n, baseline, exited=False):
    """Live, animation-only, dead, or exited -- four different claims."""
    if exited:
        return "EXITED"
    if n is None:
        return "NO WINDOW"
    if n < 0:
        return "compare failed"
    if n == 0:
        return "DEAD"
    if baseline is None or baseline < 0:
        return f"{n:,} px (no baseline)"
    # Idle drift is the floor. A key must clear it by a clear margin to count;
    # below that, the frame moved because the game animates, not because the
    # player did anything.
    threshold = max(baseline * 1.8, baseline + 250)
    if n <= threshold:
        return f"BELOW-FLOOR ({n:,} px, idle {baseline:,}) — read the box"
    return f"ABOVE-FLOOR ({n:,} px, idle {baseline:,})"


def sweep(exdir: pathlib.Path, keys, display_num, settle, outdir,
          samples=5, gap=0.18):
    need("Xvfb", "xdotool", "import", "compare")
    name = read_name(exdir)
    binary = ensure_built(exdir, name)
    if not binary.is_file():
        raise SystemExit(f"{binary} missing")

    # Clear stale instances by EXACT name. `pkill -f` would also match this
    # script and any shell whose command line contains the path.
    run(["pkill", "-x", name])
    time.sleep(0.3)

    print(f"\n=== {exdir.name}  (binary: {name}) ===")
    rows = []
    with Display(display_num) as d:
        exout = outdir / exdir.name
        baseline = measure_baseline(binary, exdir, d.env, exout, settle)
        if baseline is not None and baseline >= 0:
            print(f"  (idle drift with no input: {baseline:,} px — a key must "
                  f"clear this to count as live)")
        else:
            print("  (WARNING: no idle baseline; animation cannot be "
                  "distinguished from input on this run)")

        for key in keys:
            row = probe(binary, exdir, d.env, key, exout, settle, baseline,
                        samples, gap)
            rows.append(row)
            what = verdict(row["changed"], baseline, row["exited"])
            extra = f"  [{row['box']}]" if row.get("box") else ""
            if row["note"]:
                extra += f"  ({row['note']})"
            print(f"  {key:<8} {what}{extra}")
    return name, rows, baseline


def main():
    ap = argparse.ArgumentParser(
        description="Press each key in a fresh process and report pixel delta.")
    ap.add_argument("example", nargs="?", type=pathlib.Path,
                    help="path to the example directory")
    ap.add_argument("--all", action="store_true",
                    help="sweep every desktop example with a Makefile")
    ap.add_argument("--keys", default=",".join(DEFAULT_KEYS),
                    help="comma-separated xdotool key names")
    ap.add_argument("--settle", type=float, default=0.7,
                    help="seconds to wait after the keypress")
    ap.add_argument("--samples", type=int, default=5,
                    help="frames sampled after each keypress; the MAX delta "
                         "wins, so a jump or a transition is not missed")
    ap.add_argument("--gap", type=float, default=0.18,
                    help="seconds between samples")
    ap.add_argument("--display", type=int, default=99)
    ap.add_argument("--out", type=pathlib.Path,
                    default=pathlib.Path("/tmp/storm-sweep"))
    args = ap.parse_args()

    if not args.all and not args.example:
        ap.error("give an example directory or --all")

    keys = [k for k in (s.strip() for s in args.keys.split(",")) if k]

    if args.all:
        targets = sorted(
            p for p in (ROOT / "examples").iterdir()
            if p.is_dir() and (p / "Makefile").is_file()
            and "NAME" in (p / "Makefile").read_text()
            and p.name not in ("nx-platformer", "android-platformer",
                               "windows-platformer"))
    else:
        ex = args.example
        targets = [ex if ex.is_absolute() else ROOT / ex]

    outdir = args.out
    summary = []
    for exdir in targets:
        try:
            name, rows, base = sweep(exdir, keys, args.display,
                                      args.settle, outdir,
                                      args.samples, args.gap)
        except SystemExit as e:
            print(f"\n=== {exdir.name} ===\n  SKIPPED: {e}")
            continue
        dead, below, above, nowin, quit_keys = [], [], [], [], []
        for r in rows:
            what = verdict(r["changed"], base, r["exited"])
            if r["exited"]:
                quit_keys.append(r["key"])
            elif r["changed"] is None:
                nowin.append(r["key"])
            elif what == "DEAD":
                dead.append(r["key"])
            elif what.startswith("BELOW"):
                below.append(r["key"])
            else:
                above.append(r["key"])
        summary.append((exdir.name, name, above, below, dead, nowin, quit_keys,
                        base))

    print("\n\n=== summary ===")
    for exname, name, above, below, dead, nowin, quit_keys, base in summary:
        drift = f"{base:,} px idle" if (base or 0) >= 0 else "no baseline"
        print(f"\n  {exname}  (binary: {name}, {drift})")
        if above:
            print(f"    above floor: {', '.join(above)}")
        if below:
            print(f"    BELOW FLOOR: {', '.join(below)}   (may still be a real "
                  "overlay — look at the box and the frame)")
        if dead:
            print(f"    DEAD       : {', '.join(dead)}   (byte-identical frames)")
        if quit_keys:
            print(f"    QUIT       : {', '.join(quit_keys)}   (ended the run — "
                  "nothing after it can be swept in the same session)")
        if nowin:
            print(f"    NO WINDOW  : {', '.join(nowin)}")
    print(f"\nframes kept in {outdir}/<example>/<key>-{{before,after}}.png")
    print("Read DEAD first: a byte-identical pair after a keypress is the "
          "strongest signal\nthis tool has, and it is invisible to the spec "
          "suite.")
    print("\nTwo keys with the SAME pixel count are a lead, not a verdict. It is "
          "how one action\nends up bound twice -- but it is also just as often "
          "two symmetric actions:\nexamples/puzzle reads 16,384 for both Left "
          "and Right, because a tetromino shifted one\ncolumn either way "
          "touches the same number of pixels. Look at the frames before you "
          "call it.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
