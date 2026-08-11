# Standalone Storm! Engine v2 game.
#
# This is deliberately NOT the two-line Makefile the in-repo examples use.
# `examples/platformer/Makefile` is only:
#
#     NAME = platformer
#     include ../examples.mk
#
# which works solely because `examples.mk` and `base.mk` sit above it in the
# engine tree. A game outside the repo has neither, so it needs the real flags,
# reproduced here.

NAME    = mygame
BIN_DIR = bin
TARGET  = $(BIN_DIR)/$(NAME)

SRCS = $(wildcard src/*.cpp) $(wildcard src/**/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)

CXX = g++

# -MMD -MP generate the .d files included at the bottom, so editing a header
# rebuilds its dependents. The engine's own base.mk gained this in v1.2.2;
# without it you get silently stale objects and have to clean every build.
CXXFLAGS = -Wall -c -g -std=c++17 -MMD -MP -I/usr/local/include

# Link only what a game needs. The engine's base.mk also links lua, nfd and
# gtk+-3.0 unconditionally, but those are editor dependencies — a game does not
# need them. -Wl,-rpath lets the binary find libstormenginev2.so at run time
# without LD_LIBRARY_PATH.
LDFLAGS = -L/usr/local/lib -Wl,-rpath=/usr/local/lib
LIBS    = -lstormenginev2 -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p $(BIN_DIR)
	$(CXX) $^ $(LDFLAGS) $(LIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# Scoped to this project only. Never reuse the engine's `clean` target here:
# base.mk derives ROOT_DIR from its own realpath and deletes every *.o in the
# whole engine repository, so `make clean` from an example wipes the others.
clean:
	rm -f $(OBJS) $(DEPS)
	rm -rf $(BIN_DIR)

run: $(TARGET)
	./$(TARGET)

-include $(DEPS)

.PHONY: all clean run
