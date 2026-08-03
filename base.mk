ROOT_DIR = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

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
CCFLAGS = -Wall -c -g -fPIC -std=c++17 -Wno-reorder -Wno-unused-parameter \
	-Wno-unused-variable -Wno-unused-function $(INCLUDE) $(shell pkg-config --cflags gtk+-3.0)

# find -exec batches the deletes (no argument-list limit) and prunes trees the
# desktop build doesn't own: git internals, vendored submodules, and the
# Android build outputs (.cxx / gradle build dirs full of NDK objects).
clean:
	rm -f $(BIN_DIR)/*
	find $(ROOT_DIR) \( -name .git -o -name .cxx -o -name build -o -path '$(ROOT_DIR)/vendor/android' \) -prune -o -name '*.o' -exec rm -f {} +

%.o: %.cpp
	$(CC) $(CCFLAGS) -o $@ $<
	
memcheck:
	valgrind --log-file=valgrind.output --leak-check=yes --leak-check=full --tool=memcheck -s $(TARGET)