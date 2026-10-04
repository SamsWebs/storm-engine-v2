#!/usr/bin/env python3
"""Sabotage `common/assetPath.h` one defect at a time and count the spec
failures each one causes. A sabotage reporting 0 failures is a spec that cannot
see the bug it was written for.

Every entry is (label, old, new) applied literally, so the sabotages are diffs
you can read rather than regexes you have to trust.

WHITESPACE-INSENSITIVE MATCHING, and that is not a nicety: the first version of
this harness anchored on 4-space-indented text, clang-format reformatted the
file to 2 spaces, and all 13 anchors went stale at once. A harness that breaks
when the file is formatted is a harness that will be reported as passing when it
has not run. The same thing happened to the netSocket harness in the same
session, which is twice.

NOT IN THE MATRIX, and the reason is worth recording rather than hiding: the
empty-path guard in AssetOverrideExists has a spec and no sabotage, because
`std::ifstream("")` is ALREADY false on this platform, so removing the guard
changes nothing observable here. A sabotage that cannot fail is a lie about what
is being verified. The spec is kept, and says in its own comment that it
documents a contract rather than catching a defect.
"""
import re
import subprocess
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
TARGET = ROOT / "common" / "assetPath.h"

SABOTAGES = [
    (
        "do not strip a leading dot-slash (the defect that shipped)",
        """while (path.rfind(kDotSlash, 0) == 0) {
        path.erase(0, kDotSlash.size());
    }""",
        "    // (not stripped)",
    ),
    (
        "strip the assets prefix before the dot-slash, so neither is found",
        """static const std::string kDotSlash = "./";
    while (path.rfind(kDotSlash, 0) == 0) {
        path.erase(0, kDotSlash.size());
    }""",
        '    static const std::string kDotSlash = "./";',
    ),
    (
        "strip assets/ repeatedly, so a real assets/assets path is mangled",
        """if (path.rfind(kAssetsPrefix, 0) == 0) {
        path.erase(0, kAssetsPrefix.size());
    }""",
        """while (path.rfind(kAssetsPrefix, 0) == 0) {
        path.erase(0, kAssetsPrefix.size());
    }""",
    ),
    (
        "strip a dot-slash anywhere rather than only at the front",
        """while (path.rfind(kDotSlash, 0) == 0) {
        path.erase(0, kDotSlash.size());
    }""",
        """for (std::size_t at = path.find(kDotSlash); at != std::string::npos;
         at = path.find(kDotSlash, at + 1)) {
        path.erase(at, kDotSlash.size());
    }""",
    ),
    (
        "leave windows separators alone (pack misses on every other platform)",
        """for (char &c : path) {
        if (c == '\\\\') {
            c = '/';
        }
    }""",
        "    // (not normalised)",
    ),
    (
        "resolve a parent reference, so a name can address outside the pack",
        """if (path.rfind(kAssetsPrefix, 0) == 0) {
        path.erase(0, kAssetsPrefix.size());
    }
    return path;""",
        """if (path.rfind(kAssetsPrefix, 0) == 0) {
        path.erase(0, kAssetsPrefix.size());
    }
    static const std::string kUp = "../";
    while (path.rfind(kUp, 0) == 0) {
        path.erase(0, kUp.size());
    }
    return path;""",
    ),
    (
        "turn an empty path into a bare separator",
        """if (base.empty()) {
        return leaf;
    }""",
        """if (base.empty()) {
        return "/";
    }""",
    ),
    (
        "double the separator when the base already ends in one",
        """if (joined.back() == '/' || joined.back() == '\\\\') {
        joined += leaf;
    } else {
        joined += "/";
        joined += leaf;
    }""",
        """joined += "/";
    joined += leaf;""",
    ),
    (
        "require the caller to have normalised the path first",
        """inline std::string AssetFilePath(const std::string &gamePath,
                                 const std::string &readableBase) {
    return JoinAssetPath(readableBase, AssetPath(gamePath));
}""",
        """inline std::string AssetFilePath(const std::string &gamePath,
                                 const std::string &readableBase) {
    return JoinAssetPath(readableBase, gamePath);
}""",
    ),
    (
        "anchor an override at a different shape than the file it replaces",
        """inline std::string AssetOverridePath(const std::string &gamePath,
                                     const std::string &writableBase) {
    return JoinAssetPath(writableBase, AssetPath(gamePath));
}""",
        """inline std::string AssetOverridePath(const std::string &gamePath,
                                     const std::string &writableBase) {
    return JoinAssetPath(writableBase, "override/" + AssetPath(gamePath));
}""",
    ),
    (
        "treat an empty writable base as an override that always exists",
        """if (!writableBase.empty()) {
        const std::string candidate =""",
        """if (true) {
        const std::string candidate =""",
    ),
    (
        "stop reporting which source was chosen",
        """if (outKind != nullptr) {
        *outKind = "override";
    }""",
        "    // (silent)",
    ),
    (
        "probe with ifstream, which accepts a DIRECTORY (good() is true)",
        """std::error_code ec;
    return std::filesystem::is_regular_file(overridePath, ec) && !ec;""",
        """return true;""",
    ),
]


def _normalise(text):
    """Collapse whitespace runs, and return (normalised, index_map).

    index_map[i] is the offset in `text` that normalised offset i came from, so
    a match found in normalised space can be mapped back to a real slice to
    splice.
    """
    out = []
    offsets = []
    in_space = False
    for i, ch in enumerate(text):
        if ch.isspace():
            if in_space:
                continue
            in_space = True
            out.append(" ")
            offsets.append(i)
        else:
            in_space = False
            out.append(ch)
            offsets.append(i)
    return "".join(out), offsets


def _splice(text, old, new):
    """Replace the first whitespace-equivalent occurrence of `old` in `text`.

    Returns None when the anchor is not there. The replacement keeps the
    indentation the surrounding code already uses rather than the indentation
    the anchor happened to be written with, so clang-format still owns the
    result.
    """
    norm_text, offsets = _normalise(text)
    norm_old, _ = _normalise(old)
    at = norm_text.find(norm_old)
    if at < 0:
        return None
    start = offsets[at]
    # Extend past the last matched character to include trailing whitespace.
    end_norm = at + len(norm_old) - 1
    end = offsets[end_norm] + 1
    while end < len(text) and text[end] in " \t":
        end += 1
    if end < len(text) and text[end] == "\n":
        end += 1
    # Indent the replacement by whatever indentation the first anchor line had.
    line_start = text.rfind("\n", 0, start) + 1
    indent = text[line_start:start]
    new_text = "".join(indent + line + "\n" for line in new.rstrip("\n").split("\n")) if new else ""
    return text[:start] + new_text + text[end:]


def run():
    """Build the narrowed spec and return (failed, tail)."""
    sources = "specs/main.cpp specs/assetPath.spec.cpp " + \
              " ".join(str(p.relative_to(ROOT)) for p in sorted(ROOT.glob("common/**/*.cpp")))
    result = subprocess.run(
        ["make", "-f", "Makefile.debian", "test", "TEST_BIN=apt", f"TESTSRCS={sources}"],
        cwd=ROOT, capture_output=True, text=True,
    )
    tail = result.stdout + result.stderr
    match = re.search(r"(\d+) tests run, (\d+) succeeded, (\d+) failed", tail)
    if match:
        return int(match.group(3)), tail
    if "./bin/apt" in tail:
        return -1, tail  # crashed: the sabotage landed harder than expected
    return None, tail


def main():
    original = TARGET.read_text()
    try:
        _, baseline_tail = run()
        baseline = re.search(r"(\d+) failed", baseline_tail)
        print("baseline: " + (baseline.group(1) + " failures" if baseline else "UNKNOWN") + "\n")
        blind = []
        for label, old, new in SABOTAGES:
            sabotaged = _splice(original, old, new)
            if sabotaged is None or sabotaged == original:
                print(f"  SKIPPED  {label}\n           (anchor not found -- update the sabotage)")
                blind.append(label + " [anchor missing]")
                continue
            TARGET.write_text(sabotaged)
            failed, tail = run()
            if failed is None:
                print(f"  BUILD ERR  {label}")
                print("           " + "\n           ".join(tail.splitlines()[-5:]))
                blind.append(label)
            elif failed < 0:
                print(f"  CRASHED    {label}")
            elif failed == 0:
                print(f"  *** 0 ***  {label}")
                blind.append(label)
            else:
                print(f"  {failed:>2} failing  {label}")
        print()
        if blind:
            print(f"{len(blind)} sabotage(s) the spec could not see:")
            for b in blind:
                print(f"  - {b}")
            return 1
        print("every sabotage was caught")
        return 0
    finally:
        TARGET.write_text(original)


if __name__ == "__main__":
    sys.exit(main())
