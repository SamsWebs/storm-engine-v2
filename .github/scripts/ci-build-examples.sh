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
#
set -e

cd /opt/library

echo "==> building and installing the engine"
make target
make install

# base.mk's LIB carries -lnfd and the GTK libraries for the editor's native
# file dialogs. examples/examples.mk inherits them but no example calls NFD or
# GTK (`grep -rl 'nfd\.h\|NFD_\|gtk/' examples/*/src` is empty), so the link
# is done with LIB overridden from the command line — otherwise every example
# fails on a missing libnfd rather than on anything real.
EXAMPLE_LIB="-L/usr/local/lib -Wl,-rpath=/usr/local/lib -lSDL2 -lSDL2_image \
-lSDL2_ttf -lSDL2_mixer -lz -ltinyxml2 -llua -ldl"

for dir in examples/*/; do
  name=$(basename "$dir")
  case "$name" in
    nx-platformer | android-platformer) continue ;;
  esac
  [ -f "$dir/Makefile" ] || continue
  echo "==> building example: $name"
  # A subshell, not `make -C`: examples/examples.mk derives BIN_DIR from $(PWD),
  # which make does not update when it changes directory itself.
  (cd "$dir" && make LIB="$EXAMPLE_LIB")
done

echo "==> compiling the editor (objects only, see header comment)"
(
  cd editor
  # Named object goals rather than the default `all`, because editor/Makefile's
  # `all` would try to link.
  make $(find src -name '*.cpp' | sed 's/\.cpp$/.o/')
)

echo "==> examples and editor built"
