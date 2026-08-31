#!/bin/sh
# Fail if the installed engine's headers differ from this checkout's.
#
# The examples build against the INSTALLED engine, deliberately: they are
# samples of what a game sees, so they should exercise the shipped artifact
# rather than the working tree. base.mk's INCLUDE is -I/usr/local/include, and
# every example includes <stormengine2/...>, which exists only there.
#
# The failure mode that motivated this check is silence. An example built
# against a stale install compiles, links, runs, and tests an engine that is
# not the one you are working on. During 2.0.0's development that produced two
# wrong conclusions: examples were reported clean after being exercised
# against a version containing none of the features under test, and a build
# failure was attributed to the branch when it came from the installed copy.
#
# It gets worse rather than better from here. While the differences were
# compile-time, a mismatch surfaced as a confusing error. 2.0.0 changes type
# layouts, so a stale-header build is silent memory corruption instead.
#
# Compares only .h files, by relative path, because `make install` copies
# common/ to stormengine2/ and strips .cpp/.o/.d.

set -eu

TREE="${1:-common}"
INSTALLED="${2:-/usr/local/include/stormengine2}"

if [ ! -d "$TREE" ]; then
  echo "check-installed-engine: '$TREE' not found; run this from the repository root." >&2
  exit 1
fi

if [ ! -d "$INSTALLED" ]; then
  cat >&2 <<EOF
check-installed-engine: no engine installed at $INSTALLED

The examples build against the installed engine, not this checkout. Install it
first:

    make -f Makefile.debian && sudo make -f Makefile.debian install
EOF
  exit 1
fi

hash_headers() {
  # Relative paths are part of the hash: a header that moved is a mismatch.
  ( cd "$1" && find . -name '*.h' | LC_ALL=C sort | xargs sha256sum ) | sha256sum | cut -d' ' -f1
}

tree_hash=$(hash_headers "$TREE")
installed_hash=$(hash_headers "$INSTALLED")

if [ "$tree_hash" = "$installed_hash" ]; then
  exit 0
fi

cat >&2 <<EOF
check-installed-engine: the installed engine does not match this checkout.

    checkout ($TREE):        ${tree_hash%"${tree_hash#????????????????}"}
    installed ($INSTALLED):  ${installed_hash%"${installed_hash#????????????????}"}

The examples would build against the installed copy and silently test an engine
that is not the one you are working on. Reinstall before building them:

    make -f Makefile.debian && sudo make -f Makefile.debian install

Headers that differ:
EOF

( cd "$TREE" && find . -name '*.h' | LC_ALL=C sort ) > /tmp/.storm-tree-headers.$$
( cd "$INSTALLED" && find . -name '*.h' | LC_ALL=C sort ) > /tmp/.storm-inst-headers.$$

comm -23 /tmp/.storm-tree-headers.$$ /tmp/.storm-inst-headers.$$ \
  | sed 's/^\./    only in the checkout: /' >&2
comm -13 /tmp/.storm-tree-headers.$$ /tmp/.storm-inst-headers.$$ \
  | sed 's/^\./    only installed:      /' >&2

comm -12 /tmp/.storm-tree-headers.$$ /tmp/.storm-inst-headers.$$ | while read -r h; do
  if ! cmp -s "$TREE/$h" "$INSTALLED/$h"; then
    echo "    content differs:     ${h#./}" >&2
  fi
done

rm -f /tmp/.storm-tree-headers.$$ /tmp/.storm-inst-headers.$$
exit 1
