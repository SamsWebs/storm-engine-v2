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

# The examples build against the INSTALLED engine -- base.mk's INCLUDE is
# -I/usr/local/include, and every example includes <stormengine2/...>, which
# exists only there. That is deliberate: an example should exercise the shipped
# artifact, not the working tree.
#
# The hazard is that a stale install is silent. The example compiles, links and
# runs against an engine that is not the one you are working on. During 2.0.0's
# development that produced two wrong conclusions -- examples reported clean
# after being exercised against a version containing none of the features under
# test, and a build failure blamed on the branch that came from the installed
# copy. It gets worse from here: 2.0.0 changes type layouts, so a stale-header
# build stops being a confusing compile error and becomes silent memory
# corruption.
#
# Checked at parse time rather than as a prerequisite of $(TARGET). A
# prerequisite would race the compile under -j, exactly as this file's `clean`
# prerequisite used to (see the note above `all`), and would run after the
# objects were already built anyway.
ENGINE_MATCH := $(shell $(ROOT_DIR)/scripts/check-installed-engine.sh $(ROOT_DIR)/common >&2 || echo MISMATCH)
ifeq ($(ENGINE_MATCH),MISMATCH)
$(error installed engine does not match this checkout -- see above)
endif

run:
	$(TARGET)

$(TARGET) : $(OBJS)
	mkdir -p $(BIN_DIR)
	/usr/bin/time -f "Compilation completed in : %E" $(CC) $^ $(LIB) -lstormenginev2 -o $@