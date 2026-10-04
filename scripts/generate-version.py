#!/usr/bin/env python3
"""Generate common/version.h, and check the version sites that hand-bump it.

2.4.4. The release number was hand-written in FOUR places -- Makefile.debian,
Makefile.win, and eight `data-ver` elements in web/index.html -- and the
release workflow validated exactly ONE of them. Its "Refuse to release a
pre-release version" step reads Makefile.debian, so a tag could ship green
while the project page advertised the previous release and the download
command on that page copied a package filename for a version that no longer
existed.

Makefile.debian stays the single source, deliberately. It is the one site the
release workflow already parses, with a gate that is documented as having to
fail CLOSED, and a root VERSION file would mean rewriting that sed to
understand `$(shell cat VERSION)` -- a policy gate weakened to accommodate a
tidier layout. The other three sites become generated.

Run after bumping the version:

    python3 scripts/generate-version.py          # write all four
    python3 scripts/generate-version.py --check  # fail if any is stale

CI and scripts/release-check.py run --check, so a bump that misses a site
fails the build instead of shipping a page that lies about its own version.

Stdlib only.
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# Makefile.debian is the source. `VERSION ?= x.y.z` -- `?=` because the release
# workflow overrides it from the tag, which is correct: the tag is what is
# being built, and the tree value is what the tree believes it is.
MAKEFILE_VERSION = r"^\s*VERSION\s*:?:?\??=\s*([^\s#]+)"

SEMVER = r"^\d+\.\d+\.\d+(?:[-~][0-9A-Za-z.\-]+)?$"

HEADER_PATH = ROOT / "common" / "version.h"
BANNER = """#pragma once

// GENERATED FILE -- DO NOT EDIT.
//
// Written by scripts/generate-version.py from Makefile.debian's VERSION. Edit
// that instead and re-run the generator; the file is committed so a consumer
// of the installed engine needs no Python, and `--check` exists so a stale
// copy cannot survive that. It is the same arrangement as
// specs/compat/bridgedNames.h, and for the same reason.
//
// WHY A HEADER AT ALL. A compile flag is not a property of the artifact: the
// engine is a library and cannot print, so "which engine was this built
// against" has to be answerable by the binary itself. A game that writes its
// version into its own source, or reads it out of a makefile at runtime, is
// answering a different question than the one being asked -- and the day the
// two disagree, the disagreement is invisible.
"""


def read_version() -> str:
    text = (ROOT / "Makefile.debian").read_text(encoding="utf-8")
    m = re.search(MAKEFILE_VERSION, text, re.MULTILINE)
    if not m:
        raise SystemExit("Makefile.debian has no 'VERSION ?= x.y.z' line; "
                         "this generator reads the version from there")
    version = m.group(1).strip()
    if not re.match(SEMVER, version):
        raise SystemExit(f"Makefile.debian VERSION is '{version}', which is "
                         f"not x.y.z (optionally a -pre or ~dev suffix)")
    return version


def header_text(version: str) -> str:
    core = version.split("-")[0].split("~")[0]
    major, minor, patch = core.split(".")
    return f"""{BANNER}
namespace storm {{

// The version this translation unit was compiled against, as a string.
inline constexpr const char *kEngineVersion = "{version}";

// The numeric triple, for a consumer that needs to compare rather than print.
// Derived from the version above at generation time, so there is still only
// one place the number is written.
inline constexpr int kEngineVersionMajor = {major};
inline constexpr int kEngineVersionMinor = {minor};
inline constexpr int kEngineVersionPatch = {patch};

// The same number with a leading "v" -- "v" followed by kEngineVersion. Kept as
// its own constant rather than built at runtime so a crash banner can print it
// from a signal handler, and so nobody has to remember to prefix the "v" at
// every call site, which is the half that gets forgotten.
inline constexpr const char *kEngineVersionString = "v{version}";

inline const char *VersionString() {{ return kEngineVersionString; }}

// A build may be told a different version than the tree declared -- the
// release workflow overrides VERSION from the tag -- so a consumer that has
// the build's own idea can compare it here rather than assuming.
inline bool VersionEquals(const char *other) {{
  if (other == nullptr) {{
    return false;
  }}
  for (int i = 0; kEngineVersion[i] != '\\0' || other[i] != '\\0'; ++i) {{
    if (kEngineVersion[i] != other[i]) {{
      return false;
    }}
  }}
  return true;
}}

}} // namespace storm
"""


def makefile_win_line(version: str) -> str:
    return f"VERSION   ?= {version}"


def stamp_web(version: str, text: str) -> str:
    """Rewrite every version string the page shows, and only those.

    The page marks each one with a `data-ver` attribute precisely so this can
    be surgical. A blanket regex over 2.x.y would also rewrite the 2.0.0
    migration notes, which are correct history and must not move.
    """
    # data-ver="..." and the element text, both inside a data-ver element.
    def in_block(m):
        return re.sub(r"\d+\.\d+\.\d+(?=[-\w]*)", version, m.group(0))

    out = []
    pos = 0
    for m in re.finditer(r"<(\w+)[^>]*\bdata-ver\b[^>]*>.*?</\1>",
                         text, re.DOTALL):
        out.append(text[pos:m.start()])
        out.append(in_block(m))
        pos = m.end()
    out.append(text[pos:])
    return "".join(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true",
                    help="fail if any version site is stale; write nothing")
    args = ap.parse_args()

    version = read_version()
    targets = []

    want_header = header_text(version)
    have_header = (HEADER_PATH.read_text(encoding="utf-8")
                   if HEADER_PATH.is_file() else "")
    targets.append(("common/version.h", want_header, have_header))

    win_path = ROOT / "Makefile.win"
    win = win_path.read_text(encoding="utf-8")
    m = re.search(MAKEFILE_VERSION, win, re.MULTILINE)
    if not m:
        raise SystemExit("Makefile.win has no VERSION line to keep in step")
    want_win = win[:m.start()] + makefile_win_line(version) + win[m.end():]
    targets.append(("Makefile.win", want_win, win))

    web_path = ROOT / "web" / "index.html"
    web = web_path.read_text(encoding="utf-8")
    targets.append(("web/index.html", stamp_web(version, web), web))

    stale = [name for name, want, have in targets if want != have]
    if args.check:
        if stale:
            print("error: stale for VERSION "
                  f"{version}: {', '.join(stale)}", file=sys.stderr)
            print("Run: python3 scripts/generate-version.py", file=sys.stderr)
            return 1
        print(f"common/version.h and the other version sites agree on "
              f"{version}.")
        return 0

    for name, want, have in targets:
        if want == have:
            continue
        (ROOT / name).write_text(want, encoding="utf-8")
        print(f"wrote {name}")
    print(f"version is {version}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
