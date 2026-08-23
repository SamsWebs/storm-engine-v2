# A Storm! Engine v2 game, built against the installed engine.
#
# No engine source needed: pkg-config supplies the include path and every
# library, including the SDL2 ones. Linking only -lstormenginev2 is NOT enough
# once your game calls SDL directly -- the linker refuses with "DSO missing
# from command line".
#
#   make        build ./bin/mygame
#   make run    build and play
#   make clean

NAME := mygame

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -g $(shell pkg-config --cflags stormengine2)
LDFLAGS  := $(shell pkg-config --libs stormengine2)

SRCS := $(shell find src -name '*.cpp')
OBJS := $(SRCS:.cpp=.o)

all: bin/$(NAME)

bin/$(NAME): $(OBJS)
	@mkdir -p bin
	$(CXX) $^ $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Asset paths in the source are relative to the working directory, so the game
# has to run from here.
run: bin/$(NAME)
	./bin/$(NAME)

clean:
	rm -f $(OBJS) bin/$(NAME)

.PHONY: all run clean
