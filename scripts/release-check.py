#!/usr/bin/env python3
"""The pre-release gate: every local check, and the coverage matrix, in one run.

Track 0.5 of docs/ROADMAP.md asked for this: "the remaining half is the release
checklist, so 'all targets build' can never again be written from intent."

The reason is a specific, already-shipped defect rather than tidiness. A
`getifaddrs` call reached the flagship game's shared header and passed 5.5k
specs on Linux while Switch, Android and Windows could not build at all. The
suite was green. Nothing in the repo said which platforms had actually been
built, so "it builds" was a claim with no referent.

So this script does two things a green suite cannot:

  1. Assert the invariants that rot silently. The version is hand-written in
     FOUR places (Makefile.debian, Makefile.win, and seven `data-ver` elements
     in web/index.html). The release workflow's "Refuse to release a
     pre-release version" step reads ONE of them, so a tag can go out with
     the website advertising the previous release and CI green.

  2. Print the coverage matrix, and CHECK the coverage claims. If a workflow
     starts building the Switch or the Android example, this says so rather
     than leaving docs/RELEASING.md quietly lying about what is verified.

Run it before tagging:

    python3 scripts/release-check.py

Exit status is 0 only if every check passed. It does NOT build anything and
does NOT run the spec suite -- those are the two slow gates, and they are run
separately by the checklist in docs/RELEASING.md. What it does check is
exactly the set of things that are cheap, are currently unchecked by CI, and
have shipped broken.

Stdlib only, like the other script in this directory.
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# Tiny check framework. A list of (name, callable) so a failure in one check
# does not hide the rest -- a release gate that stops at the first problem
# makes you re-run it once per problem.
# ---------------------------------------------------------------------------

results = []


def check(name):
    def deco(fn):
        results.append((name, fn))
        return fn
    return deco


class Fail(Exception):
    pass


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def extract_version(text, pattern, rel):
    m = re.search(pattern, text, re.MULTILINE)
    if not m:
        raise Fail(f"{rel}: could not read a version (pattern {pattern!r})")
    return m.group(1)


# ---------------------------------------------------------------------------
# 1. The version sites.
# ---------------------------------------------------------------------------
# Makefile.debian and Makefile.win are `VERSION ?= x.y.z`; the `?=` is
# deliberate (the release workflow overrides it from the tag) so the pattern
# accepts `=`, `?=`, `:=` and `::=` the same way the workflow's own sed does.
MAKEFILE_VERSION = r"^\s*VERSION\s*:?:?\??=\s*([^\s#]+)"

# web/index.html marks every user-visible version string with a `data-ver`
# attribute, so the check is over the marked elements rather than a regex
# sweep for anything shaped like a version. That matters: the file also
# carries HISTORICAL versions (the 2.0.0 migration notes), and asserting
# every 2.x.y in it would fail on correct history.
DATA_VER_BLOCK = re.compile(
    r"<(\w+)[^>]*\bdata-ver\b[^>]*>(.*?)</\1>", re.DOTALL)


@check("version: the four hand-written sites agree")
def _version_sites():
    deb = extract_version(read("Makefile.debian"), MAKEFILE_VERSION,
                          "Makefile.debian")
    win = extract_version(read("Makefile.win"), MAKEFILE_VERSION,
                          "Makefile.win")

    web = read("web/index.html")
    found = {}
    elements = 0
    for m in DATA_VER_BLOCK.finditer(web):
        elements += 1
        # Search the whole element -- its text AND its attributes, since the
        # version lives in a data-copy payload as often as in the text.
        #
        # The leading assertion is `(?<![\d.])`, NOT `\b`. A word boundary
        # fails on `libstormenginev2_2.3.1_amd64.deb`, because `_` is a word
        # character and so is the `2` after it, so there is no boundary -- which
        # silently dropped the Debian install command from the check, the very
        # string a visitor copies. What must be excluded is a version that is
        # itself a continuation of a longer dotted number, not a preceding
        # underscore.
        for v in re.findall(r"(?<![\d.])v?(\d+\.\d+\.\d+)", m.group(0)):
            found[v] = found.get(v, 0) + 1
    if not found:
        raise Fail("web/index.html: no data-ver element carried a version -- "
                   "either the attribute was removed or every copy is empty; "
                   "the site would ship without a version at all")

    distinct = sorted(found)
    if distinct != [deb]:
        raise Fail(
            f"web/index.html advertises {distinct} but Makefile.debian "
            f"says {deb}. data-ver occurrences: {found}. "
            "The release workflow reads Makefile.debian ONLY, so a tag can "
            "ship with the website advertising the wrong release.")

    if win != deb:
        raise Fail(f"Makefile.win says {win}, Makefile.debian says {deb}. "
                   "Makefile.win feeds DISTNAME and the .zip name, so a local "
                   "`make dist` would produce a zip labelled for another "
                   "release. CI passes VERSION= on the command line, which is "
                   "why this never showed up there.")
    return f"{deb} across Makefile.debian, Makefile.win and web/index.html " \
           f"({elements} data-ver elements, {sum(found.values())} version " \
           f"strings)"


@check("version: not a pre-release, and not ahead of the last tag")
def _version_prerelease():
    deb = extract_version(read("Makefile.debian"), MAKEFILE_VERSION,
                          "Makefile.debian")
    if "~" in deb or "-" in deb:
        raise Fail(f"VERSION is '{deb}', a pre-release. The release workflow "
                   "refuses these, and rightly -- set a final version first.")
    tags = subprocess.run(["git", "tag", "--list", "v*.*.*"],
                          cwd=ROOT, capture_output=True, text=True).stdout
    tags = sorted(t.strip() for t in tags.splitlines() if t.strip())
    if not tags:
        raise Fail("no vX.Y.Z tags in the tree; cannot compare the version")
    last = tags[-1][1:]
    def key(v):
        return tuple(int(p) for p in v.split("."))
    if key(deb) < key(last):
        raise Fail(f"VERSION is {deb}, behind the newest tag v{last}. Releasing "
                   "would republish an older number over a newer artifact.")
    if key(deb) == key(last):
        return f"{deb} equals the newest tag v{last} (a rebuild, not a new " \
               "release -- workflow_dispatch is the right button for that)"
    return f"{deb} is ahead of the newest tag v{last}"


# ---------------------------------------------------------------------------
# 2. The source list (Track 0.4's checker, re-derived so this script is
#    runnable on its own).
# ---------------------------------------------------------------------------
@check("sources: engine-sources.txt matches find common")
def _sources():
    listed = sorted(
        ln.strip() for ln in read("engine-sources.txt").splitlines()
        if ln.strip() and not ln.strip().startswith("#"))
    actual = sorted(
        str(p.relative_to(ROOT)) for p in (ROOT / "common").rglob("*.cpp"))
    if listed != actual:
        only_listed = sorted(set(listed) - set(actual))
        only_actual = sorted(set(actual) - set(listed))
        raise Fail(f"drift. listed but absent: {only_listed or '-'}; "
                   f"present but unlisted: {only_actual or '-'}. "
                   "Regenerate with `find common -name '*.cpp' | sort`.")
    return f"{len(listed)} translation units"


# ---------------------------------------------------------------------------
# 3. The compat probe.
# ---------------------------------------------------------------------------
@check("compat: the generated bridge probe is not stale")
def _compat():
    p = subprocess.run(
        [sys.executable, "scripts/generate-compat-probes.py", "--check"],
        cwd=ROOT, capture_output=True, text=True)
    if p.returncode != 0:
        raise Fail((p.stdout + p.stderr).strip().splitlines()[-1]
                   if (p.stdout + p.stderr).strip() else "generator failed")
    return (p.stdout.strip().splitlines() or ["ok"])[-1]


# ---------------------------------------------------------------------------
# 4. The coverage claims. This is the part that makes the checklist honest
#    over time: if a workflow grows a Switch or Android build, the table in
#    docs/RELEASING.md is wrong, and this says so.
# ---------------------------------------------------------------------------
WORKFLOWS = sorted((ROOT / ".github" / "workflows").glob("*.yml"))

# Comments are stripped before matching, and that is the whole subtlety here.
# Every one of these workflow files contains prose explaining WHY a platform is
# not built -- ".dockerignore keeps both trees out of the build context",
# "devkitPro is not on this runner". Matching the raw text flags the
# documentation of the gap as if it were the gap, which is how the first
# version of this check reported that CI had grown three new platforms.
CODE_FILES = WORKFLOWS + [ROOT / ".github" / "scripts" / "ci-build-examples.sh"]


def code_only(text):
    """Drop `#` comments. YAML and sh both treat a `#` at the start of a line
    or after whitespace as a comment; the sh case is only used on
    ci-build-examples.sh, whose comments are all whole-line."""
    return "\n".join(re.sub(r"(?:^|\s)#.*$", "", ln)
                     for ln in text.splitlines())


# Platforms the matrix in docs/RELEASING.md says NOTHING builds. Asserting
# these are absent is the anti-rot check: if a Switch or NDK build ever lands,
# this fails and the table has to be corrected rather than left lying.
UNCOVERED = {
    "Switch (devkitPro)": r"devkitpro|switch_rules|libnx",
    "Android (NDK/Gradle)": r"ndk|gradlew|\bANDROID_HOME\b",
}

# Platforms the matrix says ARE covered. Asserting these are present catches
# the opposite rot: a job deleted, and a doc that still promises it.
#
# Matched on JOB IDENTITIES, not on the platform word. A first attempt matched
# `mingw`, which appears in the objdump invocations of three unrelated steps,
# so deleting the `build-windows` job's real work still passed -- the check
# could not tell the job was gone. `build-windows` and `build-linux` are
# workflow job ids: if one is renamed or removed, the promise changed and a
# human should read it.
COVERED = {
    "Windows (MinGW)": r"^\s*build-windows:",
    "Linux (.deb, amd64+arm64)": r"^\s*build-linux:",
    "Linux (PR spec image)": r"Dockerfile\.debian",
}


@check("coverage: the CI matrix matches what the docs claim")
def _coverage():
    blob = code_only("\n".join(
        f.read_text(encoding="utf-8", errors="replace") for f in CODE_FILES))

    built = sorted(n for n, p in COVERED.items()
                   if re.search(p, blob, re.IGNORECASE | re.MULTILINE))
    missing = sorted(n for n, p in COVERED.items()
                     if not re.search(p, blob, re.IGNORECASE | re.MULTILINE))
    leaked = sorted(n for n, p in UNCOVERED.items()
                    if re.search(p, blob, re.IGNORECASE))

    if leaked:
        raise Fail(
            f"CI now contains a real build invocation for {leaked} (matched "
            "outside comments), but docs/RELEASING.md lists that platform as "
            "built by nothing. Correct the table in the same change that added "
            "the build -- a checklist that understates coverage is the defect "
            "this whole script exists to prevent.")
    if missing:
        raise Fail(
            f"CI no longer builds {missing}, but docs/RELEASING.md still "
            "claims it is covered. A job was removed or renamed; the "
            "checklist is now promising verification that does not happen.")

    # Both cross-build trees are pruned from the Docker context, so no script
    # inside the image can build them even if one asked to.
    di = read(".dockerignore")
    pruned = [ex for ex in ("examples/nx-platformer",
                            "examples/android-platformer")
              if ex in di]
    if len(pruned) != 2:
        raise Fail(f".dockerignore prunes only {pruned} of the two "
                   "cross-build example trees. If that changed on purpose, "
                   "re-read what the CI image can now reach.")

    return (f"{len(built)}/{len(COVERED)} documented platforms confirmed in "
            f"{len(CODE_FILES)} CI files; {len(UNCOVERED)} unbuilt platforms "
            f"confirmed absent; {len(pruned)} trees pruned from the context")


# ---------------------------------------------------------------------------
# Report.
# ---------------------------------------------------------------------------
def main():
    print("Storm! Engine -- pre-release gate\n")
    failed = 0
    for name, fn in results:
        try:
            detail = fn()
            print(f"  PASS  {name}\n          {detail}")
        except Fail as e:
            failed += 1
            print(f"  FAIL  {name}\n          {e}")
        except Exception as e:  # noqa: BLE001 - a gate must not traceback
            failed += 1
            print(f"  ERROR {name}\n          {type(e).__name__}: {e}")

    print("\nCoverage -- what a green run does NOT prove:")
    for line in (
        "  Switch      examples/nx-platformer  built by NOTHING here or in CI",
        "  Android     examples/android-platformer  built by NOTHING here or in CI",
        "  Windows     MinGW cross-build, tag push only (never on a PR)",
        "  Examples    nine desktop trees link only in the CI Docker image",
        "  Editor      compiled to objects only; libnfd has no Debian package",
        "  Suite       NOT run by this script -- see docs/RELEASING.md step 3",
    ):
        print(line)

    print()
    if failed:
        print(f"{failed} check(s) FAILED.")
        return 1
    print("All checks passed. Two gates remain and are NOT run here:")
    print("  make -f Makefile.debian test      (the spec suite)")
    print("  the pushed tag's build-and-release run")
    return 0


if __name__ == "__main__":
    sys.exit(main())
