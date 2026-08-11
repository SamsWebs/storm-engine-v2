BIN				= $(NAME)
BIN_DIR   		= $(PWD)/bin
TARGET 			= $(BIN_DIR)/$(BIN)
DATA_PREFIX   	= $(PWD)/assets/

SRCS	= $(wildcard vendor/imgui/*.cpp)
SRCS	+= $(wildcard src/**/*.cpp)
SRCS	+= $(wildcard src/*.cpp)
OBJS 	= $(SRCS:.cpp=.o)

# No `clean` prerequisite. Makefile.debian dropped its own for the reason given
# there -- base.mk tracks headers with -MMD -MP, so the unconditional clean only
# forced a full rebuild every time -- and examples.win.mk was already written
# without one. This file and editor/Makefile were the two that kept it.
#
# Here it was worse than slow. `all: clean $(TARGET)` states no ordering between
# the two prerequisites, so under -j make runs them concurrently: clean's
# repo-wide `.o`/`.d` find-delete can land mid-compile, and its
# `rm -f $(BIN_DIR)/*` can delete the binary after the link. make still exits 0,
# so the build reports success with no executable produced. Observed 1 failure
# in 3 runs of `make -j12` in examples/shooter.
all: $(TARGET)

include ../../base.mk

run:
	$(TARGET)

$(TARGET) : $(OBJS)
	mkdir -p $(BIN_DIR)
	/usr/bin/time -f "Compilation completed in : %E" $(CC) $^ $(LIB) -lstormenginev2 -o $@