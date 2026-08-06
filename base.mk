# `:=`, not `=`. MAKEFILE_LIST grows as make reads more makefiles, and the
# -include of the generated .d files below (and the one at the bottom of
# Makefile.debian) appends every one of them to it. With a recursive `=` the
# lastword is whichever .d was read last, so ROOT_DIR silently became the
# directory of some object file the moment a second build ran: -I$(ROOT_DIR)/
# vendor pointed at nothing, `clean` walked a subtree instead of the repo, and
# the `%.o: %.cpp $(ROOT_DIR)/base.mk` rule below stopped matching at all in
# editor/ and examples/ — make fell through to its built-in flagless
# `g++ -c -o` rule, dropping -std=c++17 and every -I. Expanding once, here,
# where MAKEFILE_LIST still ends at base.mk itself, pins it to the repo root.
ROOT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

# Linux desktop toolchain, and only that. Three build entry points include this
# file — Makefile.debian, examples/examples.mk and editor/Makefile — and none
# of them cross-compiles for Windows: the MinGW build lives in the standalone
# Makefile.win (engine + specs) and examples/examples.win.mk (examples).
#
# A Windows branch was added here and removed again. It could not work from any
# of the three includers: Makefile.debian names its output .so and its run-test
# target executes the test binary natively, neither of which translates to a
# cross-built .exe. Keeping one here would mean a second copy of the flags in
# Makefile.win to keep in sync, for a path with no caller. Change Windows
# flags in Makefile.win and examples/examples.win.mk instead.
CC = g++
LIB = -L/usr/local/lib -Wl,-rpath=/usr/local/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer \
	-lz -ltinyxml2 -llua -ldl -lnfd $(shell pkg-config --libs gtk+-3.0)
INCLUDE = -I/usr/local/include -I$(ROOT_DIR)/vendor

# Build profile. The default is the local-dev one — unoptimized with debug
# info, which is what it has always been. PROFILE=release drops -g and turns
# on -O2; .github/workflows/build-and-release.yml passes it so the shipped
# .deb is no longer an -O0 build. Either knob can also be set on its own,
# e.g. `make OPT=-O1` or `make DEBUGFLAGS=-ggdb3`.
PROFILE ?= debug
ifeq ($(PROFILE),release)
OPT ?= -O2
DEBUGFLAGS ?=
else
OPT ?= -O0
DEBUGFLAGS ?= -g
endif

# -MMD -MP make the compiler write a .d file next to every .o naming the
# headers that object was compiled from; the -include below feeds them back
# to make, so editing a header now rebuilds every source that includes it.
# -MP adds an empty phony rule per header so deleting one does not wedge the
# build with "No rule to make target".
CCFLAGS = -Wall -c $(OPT) $(DEBUGFLAGS) -MMD -MP -fPIC -std=c++17 \
	-Wno-reorder -Wno-unused-parameter -Wno-unused-variable \
	-Wno-unused-function $(INCLUDE) $(shell pkg-config --cflags gtk+-3.0)

# PROFILE, OPT and DEBUGFLAGS arrive on the command line, so they touch no file
# and neither the .d files nor the base.mk prerequisite below can notice them
# changing. Without this, `make && make PROFILE=release target` recompiles
# nothing and links -O0 -g objects into the release .so — which is then
# stripped, so even the file size looks right.
#
# Hash the flags into a stamp file, rewritten only when they actually differ
# (an unconditional write would rebuild the world on every invocation), and
# make every object depend on the stamp.
FLAGSTAMP := $(ROOT_DIR)/.build-flags
$(shell new=$$(printf '%s' "$(CCFLAGS)" | md5sum); \
        [ "$$new" = "$$(cat $(FLAGSTAMP) 2>/dev/null)" ] \
          || printf '%s' "$$new" > $(FLAGSTAMP))
# Empty rule so make knows the stamp can be made. Without it, GNU make 4.3's
# implicit-rule search does not see a file created by $(shell) during the same
# invocation, so the %.o rule's prerequisite looks unsatisfiable, make falls
# back to its built-in rule, and every compile drops -std=c++17 (the CI
# failure this guards). The recipe is empty on purpose — the $(shell) above
# owns the write; this line only declares the target exists.
$(FLAGSTAMP): ;

# examples/examples.mk and editor/Makefile both define OBJS before including
# this file, so their dependency files can be pulled in from here and neither
# needs an edit. Makefile.debian builds its object lists after the include
# and does its own -include at the bottom of the file.
-include $(OBJS:.o=.d)

# find -exec batches the deletes (no argument-list limit) and prunes trees the
# desktop build doesn't own: git internals, vendored submodules, and the
# Android build outputs (.cxx / gradle build dirs full of NDK objects).
clean:
	rm -f $(BIN_DIR)/*
	find $(ROOT_DIR) \( -name .git -o -name .cxx -o -name build -o -path '$(ROOT_DIR)/vendor/android' \) -prune -o \( -name '*.o' -o -name '*.d' \) -exec rm -f {} +

# base.mk is a prerequisite so that editing a compiler flag invalidates every
# object — the .d files track headers, nothing else would notice. It also
# covers the one-time upgrade to dependency tracking: objects built before
# -MMD existed have no .d file, so nothing else would ever rebuild them.
# FLAGSTAMP covers the other half: flags overridden on the command line, which
# edit no file at all.
%.o: %.cpp $(ROOT_DIR)/base.mk $(FLAGSTAMP)
	$(CC) $(CCFLAGS) -o $@ $<

memcheck:
	valgrind --log-file=valgrind.output --leak-check=yes --leak-check=full --tool=memcheck -s $(TARGET)