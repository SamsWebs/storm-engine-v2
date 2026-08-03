# ── Windows example build (included by each example's Makefile.win) ─────────
#
# An example Makefile sets NAME and then does:
#   include ../examples.win.mk
#
# This mirrors examples.mk but cross-compiles for Windows with MinGW-w64,
# linking the engine objects directly (no installed .dll) alongside the
# vendored SDL2 deps that Makefile.win builds into build/win/deps.
#
# Prerequisites: run `make -f Makefile.win deps` from the repo root first.

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

CXXFLAGS := -std=c++17 -Wall -Wextra -O2 \
            -D_WIN32_WINNT=0x0600 \
            -I$(ROOT)/common -I$(VENDOR)/glm \
            -I$(DEPS)/include -I$(DEPS)/include/SDL2 \
            -I$(CURDIR)/src -I$(CURDIR)/include \
            -MMD -MP

LDFLAGS := -L$(DEPS)/lib -mconsole -static-libgcc -static-libstdc++
LIBS    := -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer \
           -ltinyxml2 -lws2_32

# Example sources
SRCS  := $(wildcard src/**/*.cpp)
SRCS  += $(wildcard src/*.cpp)
OBJS  := $(patsubst src/%.cpp, $(OBJDIR)/%.o, $(SRCS))

# Engine sources (linked statically — no installed .dll on Windows).
# Recursive, matching Makefile.win and Makefile.debian: a non-recursive glob
# picks up 6 of the 13 translation units and silently drops all of common/net/,
# so netchat/netrepl/netplay-checkers fail to link on every Net* symbol.
ENGINE_SRCS := $(shell find $(ENGINE_DIR) -name '*.cpp')
ENGINE_OBJS := $(patsubst $(ENGINE_DIR)/%.cpp, $(OBJDIR)/engine/%.o, $(ENGINE_SRCS))

TARGET := $(BUILD)/$(NAME).exe

# Runtime DLLs the vendored deps may import. The exe itself links
# -static-libgcc -static-libstdc++, but the SDL2 DLLs in $(DEPS)/bin do not, so
# re-enabling an SDL_image/SDL_mixer codec can add a C++ runtime import with
# nothing else to catch it — and Windows reports a missing DLL as a silent
# non-zero exit under Wine.
#
# Resolved through $(CXX) -print-file-name rather than a fixed
# /usr/$(CROSS)/lib path: /usr/lib/gcc/$(CROSS) carries both a -posix and a
# -win32 libstdc++-6.dll, and staging the win32 one beside this -posix build
# reintroduces the ABI mismatch the toolchain file exists to avoid.
RUNTIME_DLLS := libwinpthread-1.dll libstdc++-6.dll libgcc_s_seh-1.dll

DEPFILES = $(patsubst %.o,%.d,$(OBJS) $(ENGINE_OBJS))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS) $(ENGINE_OBJS)
	@mkdir -p $(BUILD)
	$(CXX) -o $@ $(OBJS) $(ENGINE_OBJS) $(LDFLAGS) $(LIBS)
	@cp -u $(DEPS)/bin/*.dll $(BUILD)/ 2>/dev/null || true
	@for dll in $(RUNTIME_DLLS); do \
	  path=$$($(CXX) -print-file-name=$$dll); \
	  if [ -f "$$path" ]; then cp -u "$$path" $(BUILD)/; fi; \
	done

$(OBJDIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/engine/%.o: $(ENGINE_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	wine64 $(TARGET)

clean:
	rm -rf $(BUILD)

-include $(DEPFILES)
