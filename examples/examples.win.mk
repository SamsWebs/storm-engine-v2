# ── Windows example build (included by each example's Makefile.win) ─────────
#
# An example Makefile sets NAME and then does:
#   include ../examples.win.mk
#
# This mirrors examples.mk but cross-compiles for Windows with MinGW-w64: it
# links build/win/libstormenginev2.dll and the vendored SDL2 deps that
# Makefile.win builds into build/win/deps.
#
# It used to COMPILE the engine into every example instead -- 13 engine
# translation units rebuilt per example, and an .exe that imported SDL2 and
# nothing of ours. That made Windows the only platform whose examples never
# exercised the shipped library: examples.mk links -lstormenginev2 on Linux,
# and the .deb and the Windows SDK zip both ship a library for consumers to
# link. Nothing built the engine one way and consumed it the other way round.
#
# The engine DLL is built on demand by the rule below, so `make -f Makefile.win`
# at the repo root is no longer a separate step you have to remember.

BUILD     := $(CURDIR)/bin/win
OBJDIR    := $(BUILD)/obj
# Derived from this file's own location, not from CURDIR: the includer is
# examples/<name>/Makefile.win, so dirname(CURDIR) lands on examples/ and every
# path below would be one level short. Anchoring to MAKEFILE_LIST also survives
# an example nested deeper than one directory.
ROOT      := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
DEPS      := $(ROOT)/build/win/deps
VENDOR    := $(ROOT)/vendor/android
ENGINE_DIR := $(ROOT)/common

CROSS := x86_64-w64-mingw32
CXX   := $(CROSS)-g++-posix

# Examples include <stormengine2/text.h>, not <text.h>, so a directory literally
# NAMED stormengine2 has to be on the include path. -I$(ROOT)/common gives the
# headers but not that prefix, which is why every example failed on its first
# engine include the first time this file was ever run.
#
# examples.mk does not need this because it builds against the INSTALLED engine
# at /usr/local/include/stormengine2. There is no install step on Windows and
# no include/ directory in this repo, so the prefix is staged here instead: a
# symlink, so it costs nothing and cannot go stale against common/.
SEINC := $(BUILD)/include

CXXFLAGS := -std=c++17 -Wall -Wextra -O2 \
            -D_WIN32_WINNT=0x0600 \
            -I$(SEINC) -I$(ROOT)/common -I$(VENDOR)/glm \
            -I$(DEPS)/include -I$(DEPS)/include/SDL2 \
            -I$(CURDIR)/src -I$(CURDIR)/include \
            -MMD -MP

ENGINE_DLL := $(ROOT)/build/win/libstormenginev2.dll

# No -static-libgcc / -static-libstdc++: see the LDFLAGS comment in the root
# Makefile.win. The engine DLL uses the shared GCC runtime, and an exe that
# statically linked its own copy would be the duplicate-unwinder bug from the
# other side.
LDFLAGS := -L$(ROOT)/build/win -L$(DEPS)/lib -mconsole
# -lstormenginev2 resolves through build/win/libstormenginev2.dll.a, the import
# library Makefile.win emits beside the DLL.
#
# SDL2 stays on the line even though the engine already links it: these examples
# call SDL directly, and the linker will not let a program borrow its library's
# transitive dependencies -- the same reason stormengine2.pc uses Requires:
# rather than Requires.private.
LIBS    := -lmingw32 -lstormenginev2 \
           -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lws2_32

# Example sources
SRCS  := $(wildcard src/**/*.cpp)
SRCS  += $(wildcard src/*.cpp)
OBJS  := $(patsubst src/%.cpp, $(OBJDIR)/%.o, $(SRCS))

TARGET := $(BUILD)/$(NAME).exe

DEPFILES = $(patsubst %.o,%.d,$(OBJS))

.PHONY: all clean run stage-runtime

all: $(TARGET)

# Built by the root Makefile.win, not duplicated here. Order-only would be
# wrong: if the engine changes, the example must relink against the new DLL.
$(ENGINE_DLL):
	$(MAKE) -C $(ROOT) -f Makefile.win $(patsubst $(ROOT)/%,%,$(ENGINE_DLL))

$(TARGET): $(OBJS) $(ENGINE_DLL)
	@mkdir -p $(BUILD)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)
	@cp -u $(ENGINE_DLL) $(DEPS)/bin/*.dll $(BUILD)/ 2>/dev/null || true
	@$(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) NAME=$(NAME) stage-runtime

# THE STAGED SET IS DERIVED, NOT LISTED. A hardcoded list shipped
# libgcc_s_seh-1.dll and libstdc++-6.dll beside every example that imports
# neither -- both exes and the engine DLL link -static-libgcc -static-libstdc++
# -- while being exactly the mechanism that once shipped libwinpthread-1.dll
# WITHOUT libstdc++-6.dll and produced a bare "exit 53" under Wine. Read the
# real import table instead, the way the root Makefile.win's stage-dlls does.
#
# Resolution goes through $(CXX) -print-file-name so the runtime belongs to the
# compiler in use: /usr/lib/gcc/$(CROSS) carries both a -posix and a -win32
# libstdc++-6.dll, and staging the win32 one beside a -posix build reintroduces
# the ABI mismatch the toolchain file exists to avoid.
#
# Three passes because a DLL staged in one pass brings imports of its own.
stage-runtime:
	@cd $(BUILD) && \
	imports() { \
	  for f in $(NAME).exe *.dll; do \
	    [ -f "$$f" ] || continue; \
	    $(CROSS)-objdump -p "$$f" 2>/dev/null | awk '/DLL Name/ {print $$3}'; \
	  done | sort -u; \
	}; \
	runtime() { \
	  p=$$($(CXX) -print-file-name="$$1" 2>/dev/null); \
	  if [ -f "$$p" ]; then echo "$$p"; return 0; fi; \
	  find /usr/lib/gcc/$(CROSS) /usr/$(CROSS) -name "$$1" 2>/dev/null | head -1; \
	}; \
	for pass in 1 2 3; do \
	  found=0; \
	  for n in $$(imports); do \
	    [ -f "$$n" ] && continue; \
	    src=$$(runtime "$$n"); \
	    if [ -n "$$src" ]; then cp -u "$$src" .; found=1; fi; \
	  done; \
	  [ $$found -eq 0 ] && break; \
	done; \
	true

# Order-only (after the |): the symlink's timestamp must not make every object
# look out of date, but it has to exist before the first compile.
$(SEINC)/stormengine2:
	@mkdir -p $(SEINC)
	@ln -sfn $(ENGINE_DIR) $@

$(OBJDIR)/%.o: src/%.cpp | $(SEINC)/stormengine2
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/engine/%.o: $(ENGINE_DIR)/%.cpp | $(SEINC)/stormengine2
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	wine64 $(TARGET)

clean:
	rm -rf $(BUILD)

-include $(DEPFILES)
