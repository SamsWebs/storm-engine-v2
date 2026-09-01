# ── windows-platformer — the acceptance test for the shipped Windows SDK ────
#
# Every other example builds against the REPO: examples.win.mk points -I at
# common/ and -L at build/win. This one builds against the UNZIPPED SDK ZIP,
# the way somebody who downloaded a release does, and nothing else in the tree
# does that. If the zip is missing a header, a DLL or the import library, this
# is what fails -- and it fails at the same point a user would hit it.
#
# IT DELIBERATELY HAS NO SOURCES OF ITS OWN. It compiles ../platformer/src.
# A copy would have been the obvious way to make a "windows-platformer", and it
# would rot: two copies of one game, and the one nobody plays is the one that
# silently stops matching. What is being demonstrated here is not a different
# game, it is a different way of CONSUMING the engine, so the game should be
# byte-identical to the example it mirrors.
#
#   make -f Makefile.win dist       # from the repo root, produces the zip
#   cd examples/windows-platformer
#   make                            # unzips the SDK and builds against it
#   make run                        # under wine
#
# Point it at an already-unpacked SDK instead with:
#   make SDK=/path/to/stormengine2-2.1.1-win64

NAME    := windows-platformer
VERSION ?= 2.1.1

ROOT    := $(abspath ../..)
UPSTREAM := ../platformer
ZIP     := $(ROOT)/build/win/stormengine2-$(VERSION)-win64.zip
# Unpacked here rather than pointing into build/win directly: the whole point is
# to consume what the ZIP contains, so a file the recipe forgot to package
# cannot be picked up from the build tree by accident.
SDK     ?= $(CURDIR)/bin/sdk/stormengine2-$(VERSION)-win64

BUILD   := $(CURDIR)/bin/win
OBJDIR  := $(BUILD)/obj

CROSS := x86_64-w64-mingw32
CXX   := $(CROSS)-g++-posix

# ONLY the SDK's include directory. No -I into common/, no -I into
# build/win/deps: if <stormengine2/...>, <SDL2/...> or <glm/...> is not in the
# zip, this must not compile.
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -D_WIN32_WINNT=0x0600 \
            -I$(SDK)/include -I$(UPSTREAM)/src -MMD -MP

LDFLAGS := -L$(SDK)/lib -mconsole
LIBS    := -lmingw32 -lstormenginev2 \
           -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lws2_32

SRCS := $(shell find $(UPSTREAM)/src -name '*.cpp')
OBJS := $(patsubst $(UPSTREAM)/src/%.cpp, $(OBJDIR)/%.o, $(SRCS))

TARGET := $(BUILD)/$(NAME).exe

.PHONY: all run clean

all: $(TARGET)

# Unpacked from the zip, so a missing file in the PACKAGE is what breaks.
$(SDK)/lib/libstormenginev2.dll.a: $(ZIP)
	@mkdir -p $(dir $(SDK))
	@rm -rf $(SDK)
	@unzip -q $(ZIP) -d $(dir $(SDK))
	@touch $@

$(ZIP):
	@echo "No SDK zip at $(ZIP)."
	@echo "Build it first:  make -f Makefile.win dist VERSION=$(VERSION)"
	@false

$(OBJDIR)/%.o: $(UPSTREAM)/src/%.cpp | $(SDK)/lib/libstormenginev2.dll.a
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# The SDK's own bin/ is copied wholesale. A consumer is told to do exactly this
# ("put the contents of bin\ beside your .exe"), so the example does it too --
# if that instruction is wrong, this run is where it shows.
$(TARGET): $(OBJS)
	@mkdir -p $(BUILD)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)
	@cp -u $(SDK)/bin/*.dll $(BUILD)/
	@cp -r $(UPSTREAM)/assets $(BUILD)/ 2>/dev/null || true

run: all
	cd $(BUILD) && wine $(NAME).exe

clean:
	rm -rf $(BUILD)

-include $(patsubst %.o,%.d,$(OBJS))
