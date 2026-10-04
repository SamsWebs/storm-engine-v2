#!/usr/bin/env python3
"""Sabotage the `.map` version header and its loader, one defect at a time, and
count the spec failures each one causes.

A sabotage reporting 0 failures is a spec that cannot see the bug it was written
for -- which is the whole reason this file exists rather than a note saying the
specs were read carefully.

Each entry is (label, file, old, new) applied literally, so the sabotages are
diffs you can read rather than regexes you have to trust. Two of them target
`common/tilemapFormat.h` and the rest `common/tilemapLoader.cpp`.
"""
import re
import subprocess
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
FILES = ["common/tilemapFormat.h", "common/tilemapLoader.cpp"]
HEADER = FILES[0]
LOADER = FILES[1]

SABOTAGES = [
    (
        "accept any version at all (the refusal is the whole point)",
        HEADER,
        """    if (value > kTileMapFormatVersion) {
      result.state = TileMapVersionState::TooNew;
      return result;
    }
    if (value < kTileMapFormatMinVersion) {
      result.state = TileMapVersionState::TooOld;
      return result;
    }
""",
        "",
    ),
    (
        "treat a newer format as the current one instead of refusing it",
        HEADER,
        """    if (value > kTileMapFormatVersion) {
      result.state = TileMapVersionState::TooNew;
      return result;
    }""",
        """    if (value > kTileMapFormatVersion) {
      result.state = TileMapVersionState::Supported;
      result.version = kTileMapFormatVersion;
      return result;
    }""",
    ),
    (
        "treat a malformed version as an old map and fall through to the old parser",
        HEADER,
        """    if (trimmed.empty() ||
        trimmed.find_first_not_of("0123456789") != std::string::npos) {
      result.state = TileMapVersionState::Malformed;
      return result;
    }""",
        """    if (trimmed.empty() ||
        trimmed.find_first_not_of("0123456789") != std::string::npos) {
      return result;
    }""",
    ),
    (
        "accept a negative version as a number",
        HEADER,
        """    if (value < kTileMapFormatMinVersion) {""",
        """    if (value < -1000000) {""",
    ),
    (
        "eat the probed token instead of rewinding (parses one field to the left)",
        HEADER,
        """        if (start != std::streampos(-1)) {
            in.clear();
            in.seekg(start);
        }
        return result;""",
        "        return result;",
    ),
    (
        "match the magic as a prefix, so a group named storm-maps loses a record",
        HEADER,
        "    if (token != kTileMapFormatMagic) {",
        "    if (token.compare(0, std::string(kTileMapFormatMagic).size(),\n"
        "                      kTileMapFormatMagic) != 0) {",
    ),
    (
        "omit the newline from the line a writer writes",
        HEADER,
        """  return std::string(kTileMapFormatMagic) + " " +
         std::to_string(kTileMapFormatVersion) + "\\n";""",
        """  return std::string(kTileMapFormatMagic) + " " +
         std::to_string(kTileMapFormatVersion);""",
    ),
    (
        "stamp a version the reader does not enforce (writer and reader drift apart)",
        HEADER,
        """         std::to_string(kTileMapFormatVersion) + "\\n";""",
        """         std::to_string(kTileMapFormatVersion + 1) + "\\n";""",
    ),
    (
        "go back to the clean-EOF tolerance: a missing flag mid-file empties the map",
        LOADER,
        """    std::string flagToken;
    if (nextToken(flagToken)) {
      if (looksNumeric(flagToken)) {
        animatedFlag = std::atoi(flagToken.c_str());
      } else {
        // Not a flag: it is the next record's group, and the loop needs it.
        pending = flagToken;
        hasPending = true;
      }
    }""",
        """    if (!(fmap >> animatedFlag)) {
      if (!fmap.eof()) {
        reportTruncated("animation flag for '" + assetId + "'");
        return;
      }
      animatedFlag = 0;
    }""",
    ),
    (
        "consume the next record's group token instead of pushing it back",
        LOADER,
        """        pending = flagToken;
        hasPending = true;""",
        """        // (token dropped)""",
    ),
    (
        "swallow the refusal and load anyway",
        LOADER,
        """  if (!refusal.empty()) {
    logger.Err(refusal);
    return;
  }""",
        """  (void)refusal;""",
    ),
    (
        "refuse maps that have no version header, breaking every existing map",
        LOADER,
        """  if (!refusal.empty()) {
    logger.Err(refusal);
    return;
  }""",
        """  if (!refusal.empty()) {
    logger.Err(refusal);
    return;
  }
  if (version.state == TileMapVersionState::Unversioned) {
    logger.Err("TileMapLoader: '" + fileMap +
               "': no version header; refusing every map written before the "
               "header existed");
    return;
  }""",
    ),
]


def run():
    """Build the narrowed specs and return (ran, failed)."""
    sources = "specs/main.cpp specs/tilemapFormat.spec.cpp " \
              "specs/tilemapLoaderEditor.spec.cpp specs/tilemapLoader.spec.cpp " + \
              " ".join(str(p.relative_to(ROOT)) for p in sorted(ROOT.glob("common/**/*.cpp")))
    result = subprocess.run(
        ["make", "-f", "Makefile.debian", "test", "TEST_BIN=tm", f"TESTSRCS={sources}"],
        cwd=ROOT, capture_output=True, text=True,
    )
    tail = result.stdout + result.stderr
    match = re.search(r"(\d+) tests run, (\d+) succeeded, (\d+) failed", tail)
    if match:
        return int(match.group(1)), int(match.group(3)), tail
    # No summary line. If the test binary ran, it CRASHED -- and a crash is the
    # sabotage landing harder than expected, not a build error, so it must not
    # be reported as "could not see". A real build failure never reaches the
    # binary: the g++ line is the last thing in the log.
    if "./bin/tm" in tail:
        return 0, -1, tail  # -1 means "crashed", which counts as caught
    return None, None, tail


def main():
    original = {f: (ROOT / f).read_text() for f in FILES}
    try:
        _, baseline_failed, _ = run()
        print(f"baseline: {baseline_failed} failures\n")
        blind = []
        for label, filename, old, new in SABOTAGES:
            if old not in original[filename]:
                print(f"  SKIPPED  {label}\n           (anchor not found in {filename})")
                blind.append(label + " [anchor missing]")
                continue
            for f in FILES:
                (ROOT / f).write_text(
                    original[f].replace(old, new, 1) if f == filename else original[f])
            _, failed, tail = run()
            if failed is None:
                print(f"  BUILD ERR  {label}")
                print("           " + "\n           ".join(tail.splitlines()[-6:]))
                blind.append(label)
            elif failed < 0:
                print(f"  CRASHED    {label}")
                print("             (the suite ran and died -- a regression this "
                      "deep reaches specs that index a map without checking its size)")
            elif failed == 0:
                print(f"  *** 0 ***  {label}")
                blind.append(label)
            else:
                print(f"  {failed:>2} failing  {label}")
        print()
        if blind:
            print(f"{len(blind)} sabotage(s) the specs could not see:")
            for b in blind:
                print(f"  - {b}")
            return 1
        print("every sabotage was caught")
        return 0
    finally:
        for f in FILES:
            (ROOT / f).write_text(original[f])


if __name__ == "__main__":
    sys.exit(main())
