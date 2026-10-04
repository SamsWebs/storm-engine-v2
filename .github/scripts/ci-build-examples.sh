#!/bin/sh
#
# Build every desktop example and compile the editor against a freshly built
# engine. Meant to run inside the Dockerfile.debian image:
#
#   docker run --rm -i <image> sh -s < .github/scripts/ci-build-examples.sh
#
# Before this existed, CI compiled neither examples/ nor editor/, so an engine
# change could break all eleven example trees and still ship a green .deb.
#
# What is covered and what is not:
#
#   * examples/{jrpg,netchat,netplay-checkers,netrepl,platformer,puzzle,
#     shooter,sports,strategy} — compiled AND linked, via their own Makefiles.
#   * editor/ — compiled to objects only. It is the one tree that genuinely
#     calls NFD_* (editor/src/utilities/FileDialogWin.cpp) and libnfd has no
#     Debian package; vendor/nfd ships nfd.h and a LICENSE, no implementation.
#     So the editor link step cannot run here. Its vendored ImGui objects are
#     skipped too — third-party code, and it doubles the compile time.
#   * examples/nx-platformer (devkitPro) and examples/android-platformer
#     (Android NDK + six submodules) — NOT covered. Neither toolchain is in
#     this image and .dockerignore keeps both trees out of the build context.
#   * examples/windows-platformer — NOT covered, and unlike those two it IS in
#     the build context, so it has to be skipped by name below. It is a Windows
#     example: it builds against the unzipped SDK zip with MinGW-w64, neither of
#     which exists here. build-and-release.yml's build-windows job builds it, on
#     the runner rather than in this image.
#
set -e

cd /opt/library

echo "==> building and installing the engine"
make target
make install

# The examples link with base.mk's LIB, unmodified. This file used to override
# it on the command line, and that override was a second copy of the flags that
# nobody kept in sync: it still named -ltinyxml2 and -llua after base.mk had
# deliberately dropped both, and it had lost the gtk pkg-config half. It died
# the moment the gate above it stopped failing first -- "cannot find
# -ltinyxml2" on the first example, with a correct list right there.
#
# The override existed to keep -lnfd off the examples' link line. -lnfd has been
# in EDITOR_LIB, not LIB, for a while now, so the reason it gave was already
# stale when the copy was written. base.mk is the one place these flags exist;
# the gtk libraries in LIB are harmless here (no example calls gtk, but they
# resolve, and the image installs them) and they are the price of not keeping a
# second copy.

for dir in examples/*/; do
  name=$(basename "$dir")
  case "$name" in
    nx-platformer | android-platformer | windows-platformer) continue ;;
  esac
  [ -f "$dir/Makefile" ] || continue
  echo "==> building example: $name"
  # A subshell, not `make -C`: examples/examples.mk derives BIN_DIR from $(PWD),
  # which make does not update when it changes directory itself.
  (cd "$dir" && make)
done

echo "==> compiling the editor (objects only, see header comment)"
(
  cd editor
  # Named object goals rather than the default `all`, because editor/Makefile's
  # `all` would try to link.
  make $(find src -name '*.cpp' | sed 's/\.cpp$/.o/')
)

echo "==> examples and editor built"
