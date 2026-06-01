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

# Prepend our local include dir so it shadows any installed headers with bugs
CCFLAGS := -I$(PWD)/include $(CCFLAGS)

clean:
	rm -f $(TARGET) && rm -f $(shell find . -name "*.o")

run:
	$(TARGET)

$(TARGET) : $(OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $^ $(LIB) -lstormenginev2 -o $@
	/usr/bin/time -f "Compilation completed in : %E" $(TARGET)