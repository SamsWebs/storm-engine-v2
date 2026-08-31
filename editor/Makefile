BIN				= editor
BIN_DIR   		= $(PWD)/bin
TARGET 			= $(BIN_DIR)/$(BIN)
DATA_PREFIX   	= $(PWD)/assets/

SRCS	= $(wildcard ../vendor/imgui/*.cpp)
SRCS	+= $(wildcard src/**/*.cpp)
SRCS	+= $(wildcard src/*.cpp)
OBJS 	= $(SRCS:.cpp=.o)

# See the note in examples/examples.mk: `all: clean $(TARGET)` orders nothing,
# so under -j clean races the build and can delete objects mid-compile or the
# binary after the link, while make still exits 0. Header tracking in base.mk
# makes the unconditional clean unnecessary anyway.
all: $(TARGET)

include ../base.mk

# sol2 includes <lua.h> unqualified, but base.mk's INCLUDE only reaches
# $(ROOT_DIR)/vendor, so only <lua/lua.h> would resolve. vendor/lua is
# headers-only and nothing links Lua -- it is here because sol2 needs the
# declarations to compile. See vendor/MANIFEST.md.
#
# Editor-only on purpose: no example includes sol, and adding this to the
# shared INCLUDE would put a dependency on every example's compile line that
# none of them uses.
INCLUDE += -I$(ROOT_DIR)/vendor/lua

# editor/include used to shadow the installed engine headers here. It held one
# byte-identical copy of common/components/sprite.h and was deleted (P45): the
# shadow could only ever hide upstream fixes. This repo is upstream — fix
# headers in common/ instead of overriding them.

clean:
	rm -f $(TARGET) && rm -f $(shell find . -name "*.o")

run:
	$(TARGET)

$(TARGET) : $(OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $^ $(LIB) $(EDITOR_LIB) -lstormenginev2 -o $@
	/usr/bin/time -f "Compilation completed in : %E" $(TARGET)