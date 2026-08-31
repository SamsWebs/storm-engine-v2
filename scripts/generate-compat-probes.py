#!/usr/bin/env python3
"""Generate specs/compat/bridgedNames.h from the engine headers.

<stormengine2/compat/global.h> has to re-export every public engine name, and
the spec that was written to prove it named ~33 of 131 by hand -- so a name
forgotten in the bridge was forgotten in the spec too, and the spec passed.
The list has to come from the ENGINE, not from the bridge, or the check is
circular.

Each name becomes `using ::Name;` inside a probe namespace. That form is legal
for every kind of entity -- class, struct, enum, enumerator, alias, variable,
function (all overloads at once) and template -- and fails to compile when the
name is not present at global scope. So a name added to the engine and not to
the bridge breaks the build with `no member named 'X' in the global namespace`.

Run after adding a public engine name:

    python3 scripts/generate-compat-probes.py

CI re-runs it and fails if the committed file differs, so it cannot go stale.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "specs" / "compat" / "bridgedNames.h"

# Names declared at namespace scope in `namespace storm`. Every header in
# common/ puts these at column 0 and indents class members, which is what makes
# a line-oriented scan reliable here; the brace depth is tracked as well so a
# reformatted file degrades into missing names (a loud compile error in the
# generated probe) rather than into silently wrong ones.
DECL = [
    re.compile(r"^(?:class|struct)\s+([A-Za-z_]\w*)\s*(?:final\s*)?[:{]"),
    re.compile(r"^enum\s+(?:class\s+)?([A-Za-z_]\w*)\s*[:{]"),
    re.compile(r"^using\s+([A-Za-z_]\w*)\s*="),
    re.compile(r"^typedef\s+.*?\b([A-Za-z_]\w*)\s*;"),
    re.compile(r"^(?:inline\s+)?constexpr\s+[\w:<>,\s*&]+?\b([A-Za-z_]\w*)\s*="),
    re.compile(r"^(?:inline\s+)?(?:const\s+)?[\w:<>,\s*&]+?\b([A-Za-z_]\w*)\s*\([^)]*\)\s*(?:const\s*)?[;{]"),
]

def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(re.sub(r"//.*", "", line) for line in text.split("\n"))

def scan(path):
    """Namespace-scope names declared in `namespace storm` in one header."""
    names, enumerators = set(), set()
    lines = strip_comments(path.read_text()).split("\n")
    inside, depth = False, 0
    pending = ""          # logical statement, joined across lines
    pending_col0 = False
    for raw in lines:
        line = raw.rstrip()
        stripped = line.strip()
        if not inside:
            if stripped.startswith("namespace storm"):
                inside, depth = True, 0
            continue
        if stripped.startswith("#"):
            continue

        if stripped and (depth == 0 or
                         re.match(r"^enum\s+(?!class\b)", pending)):
            if not pending:
                pending_col0 = bool(line) and not line[0].isspace()
            pending = (pending + " " + stripped).strip()

        opened = line.count("{")
        closed = line.count("}")

        # A statement ends at `;` or at the `{` that opens a body/definition.
        # An unscoped enum's enumerators are namespace-scope names, and the
        # body usually spans lines -- so unlike every other declaration, this
        # statement must not be flushed at its opening brace.
        collecting_enum = bool(re.match(r"^enum\s+(?!class\b)", pending)) and \
            "}" not in pending
        # A completed unscoped enum flushes on its own, independent of depth:
        # the closing `};` is processed while depth is still 1 (it is decremented
        # below), so a depth==0 test would carry the enum forward and swallow
        # whatever declaration follows it.
        enum_complete = bool(re.match(r"^enum\s+(?!class\b)", pending)) and \
            "}" in pending and ";" in pending
        if pending and (enum_complete or
                        (depth == 0 and not collecting_enum and
                         (";" in stripped or opened))):
            if pending_col0:
                statement = pending
                # `template <typename T> class Component : ...` -- strip the
                # prefix so the class/struct pattern sees the declaration. A
                # using-declaration names a template without arguments, so
                # nothing else is needed for these.
                statement = re.sub(r"^template\s*<[^>]*>\s*", "", statement)
                for pattern in DECL:
                    m = pattern.match(statement)
                    if m:
                        name = m.group(1)
                        # `void Registry::AddSystem(...)` is an out-of-line
                        # member definition, not a new namespace-scope name.
                        # Match on `Class::name`, not on a bare `::` anywhere,
                        # which would also reject `using Map = std::vector<T>`.
                        if not re.search(r"\b\w+::\s*" + re.escape(name) + r"\b",
                                         statement):
                            names.add(name)
                        break
                m = re.match(r"^enum\s+(?!class\b)([A-Za-z_]\w*)", statement)
                if m and "{" in statement and "}" in statement:
                    inner = statement[statement.index("{") + 1:statement.rindex("}")]
                    for item in inner.split(","):
                        item = item.split("=")[0].strip()
                        if re.fullmatch(r"[A-Za-z_]\w*", item):
                            enumerators.add(item)
            pending = ""

        depth += opened - closed
        if depth < 0:
            inside, depth = False, 0
        if depth > 0 and not re.match(r"^enum\s+(?!class\b)", pending):
            pending = ""
    return names, enumerators

def main():
    names, enumerators = set(), set()
    for path in sorted((ROOT / "common").rglob("*.h")):
        if "compat" in path.parts:
            continue
        n, e = scan(path)
        names |= n
        enumerators |= e
    # Templates are named without arguments by a using-declaration, so they
    # need no special handling here.
    all_names = sorted(names | enumerators)

    body = [
        "// GENERATED by scripts/generate-compat-probes.py -- do not edit.",
        "//",
        "// One `using ::Name;` per public engine name, taken from the engine",
        "// headers rather than from the bridge. Including this after",
        "// <stormengine2/compat/global.h> fails to compile if the bridge does",
        "// not re-export a name, which is the property the compat spec claims",
        "// and could not previously deliver: the hand-written list named a",
        "// third of the exports, so a name missing from the bridge was missing",
        "// from the spec too.",
        "//",
        f"// {len(all_names)} names.",
        "#pragma once",
        "",
        "namespace storm_compat_probe {",
    ]
    body += [f"using ::{name};" for name in all_names]
    body += ["} // namespace storm_compat_probe", ""]
    text = "\n".join(body)

    if "--check" in sys.argv:
        current = OUT.read_text() if OUT.exists() else ""
        if current != text:
            print(f"{OUT} is stale. Run: python3 scripts/generate-compat-probes.py")
            sys.exit(1)
        print(f"{OUT} is up to date ({len(all_names)} names).")
        return
    OUT.write_text(text)
    print(f"wrote {OUT} ({len(all_names)} names)")

if __name__ == "__main__":
    main()
