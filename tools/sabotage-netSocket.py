#!/usr/bin/env python3
"""Sabotage `common/net/netSocket.cpp` one defect at a time and count the
spec failures each one causes. A sabotage that reports 0 failures is a spec
that cannot see the bug it was written for.

Each entry is (label, file, old, new) applied literally, so the sabotages are
diffs you can read rather than regexes you have to trust. Two of them target
the header, because the Result decoders live there.
"""
import re
import subprocess
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
# The two files a defect in this slice can live in. The Result decoders are
# inline constexpr in the header (so the Switch build's static_asserts can see
# them) and everything else is in the .cpp, so a single-file harness would
# silently skip the two most important sabotages.
FILES = ["common/net/netSocket.h", "common/net/netSocket.cpp"]
BACKUP = {f: ROOT / (f + ".sabotage-backup") for f in FILES}

SABOTAGES = [
    (
        "collapse all three messages into one (the exact pre-slice regression)",
        "common/net/netSocket.cpp",
        '''  case NetSocketOpenFailure::PlatformInit:
    // Deliberately says nothing about sockets. This is the line a Switch
    // developer sees when libnx was never started, and the whole reason the
    // three messages are separate is that the old shared line sent them
    // looking for a firewall.
    return "NetSocket: networking stack init failed (WSAStartup on Windows, "
           "socketInitializeDefault on Switch) before any socket call";
  case NetSocketOpenFailure::Socket:
    return "NetSocket: failed to create non-blocking UDP socket";''',
        '''  case NetSocketOpenFailure::PlatformInit:
  case NetSocketOpenFailure::Socket:
    return "NetSocket: failed to create non-blocking UDP socket";''',
    ),
    (
        "classify a failed non-blocking switch as no failure at all",
        "common/net/netSocket.cpp",
        "  if (!socketOk || !nonBlockingOk)\n    return NetSocketOpenFailure::Socket;",
        "  if (!socketOk)\n    return NetSocketOpenFailure::Socket;",
    ),
    (
        "decode a Result with the 0x3FF layout a person would guess",
        "common/net/netSocket.h",
        "inline constexpr uint32_t NetResultModule(uint32_t result) {\n  return result & 0x1FFu;\n}",
        "inline constexpr uint32_t NetResultModule(uint32_t result) {\n  return result & 0x3FFu;\n}",
    ),
    (
        "stop splitting the description at bit 9",
        "common/net/netSocket.h",
        "inline constexpr uint32_t NetResultDescription(uint32_t result) {\n  return (result >> 9) & 0x1FFFu;\n}",
        "inline constexpr uint32_t NetResultDescription(uint32_t result) {\n  return (result >> 10) & 0x1FFFu;\n}",
    ),
    (
        "say nothing for an unrecognised stage (the silence this slice removes)",
        "common/net/netSocket.cpp",
        '  return "NetSocket: socket setup failed for an unrecognised reason";',
        '  return std::string();',
    ),
    (
        "report an ephemeral bind as 'port 0' like every other port",
        "common/net/netSocket.cpp",
        '''    if (port == 0)
      return "NetSocket: failed to bind an OS-assigned UDP port";
    return "NetSocket: failed to bind UDP port " + std::to_string(port);''',
        '''    return "NetSocket: failed to bind UDP port " + std::to_string(port);''',
    ),
    (
        "drop the shared prefix from the bind line only",
        "common/net/netSocket.cpp",
        '    return "NetSocket: failed to bind UDP port " + std::to_string(port);',
        '    return "failed to bind UDP port " + std::to_string(port);',
    ),
    (
        "drop the port from the bind line",
        "common/net/netSocket.cpp",
        '    return "NetSocket: failed to bind UDP port " + std::to_string(port);',
        '    return "NetSocket: failed to bind a UDP port";',
    ),
    (
        "print the Result code in decimal behind an 0x prefix",
        "common/net/netSocket.cpp",
        'std::snprintf(code, sizeof(code), "0x%04X", result);',
        'std::snprintf(code, sizeof(code), "0x%04u", result);',
    ),
    (
        "treat a zero Result as a failure and print it",
        "common/net/netSocket.cpp",
        "  if (result == 0)\n    return std::string();",
        "  if (result == 1)\n    return std::string();",
    ),
    (
        "log a message for the success case too",
        "common/net/netSocket.cpp",
        "  case NetSocketOpenFailure::None:\n    return std::string();",
        '  case NetSocketOpenFailure::None:\n    return "NetSocket: opened";',
    ),
]


def run():
    """Build the narrowed spec and return (ran, failed)."""
    result = subprocess.run(
        [
            "make", "-f", "Makefile.debian", "test", "TEST_BIN=ns",
            'TESTSRCS=specs/main.cpp specs/net/netSocket.spec.cpp '
            + " ".join(str(p.relative_to(ROOT)) for p in sorted(ROOT.glob("common/**/*.cpp"))),
        ],
        cwd=ROOT, capture_output=True, text=True,
    )
    tail = result.stdout + result.stderr
    match = re.search(r"(\d+) tests run, (\d+) succeeded, (\d+) failed", tail)
    if not match:
        return None, None, tail
    return int(match.group(1)), int(match.group(3)), tail


def main():
    original = {f: (ROOT / f).read_text() for f in FILES}
    for f in FILES:
        BACKUP[f].write_text(original[f])
    try:
        _, baseline_failed, _ = run()
        print(f"baseline: {baseline_failed} failures\n")
        blind = []
        for label, filename, old, new in SABOTAGES:
            path = ROOT / filename
            if old not in original[filename]:
                print(f"  SKIPPED  {label}\n           (anchor not found in {filename} -- update the sabotage)")
                blind.append(label + " [anchor missing]")
                continue
            for f in FILES:
                path = ROOT / f
                path.write_text(original[f].replace(old, new, 1)
                                if f == filename else original[f])
            _, failed, tail = run()
            if failed is None:
                print(f"  BUILD ERR  {label}")
                print("           " + "\n           ".join(tail.splitlines()[-6:]))
                blind.append(label)
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
        for f in FILES:
            (ROOT / f).write_text(original[f])
            BACKUP[f].unlink()


if __name__ == "__main__":
    sys.exit(main())
