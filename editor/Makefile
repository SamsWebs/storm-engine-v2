BIN				= editor
BIN_DIR   		= $(PWD)/bin
TARGET 			= $(BIN_DIR)/$(BIN)
DATA_PREFIX   	= $(PWD)/assets/

SRCS	= $(wildcard ../vendor/imgui/*.cpp)
SRCS	+= $(wildcard src/**/*.cpp)
SRCS	+= $(wildcard src/*.cpp)
OBJS 	= $(SRCS:.cpp=.o)

all: clean $(TARGET)

include ../base.mk

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
	$(CC) $^ $(LIB) -lstormenginev2 -o $@
	/usr/bin/time -f "Compilation completed in : %E" $(TARGET)