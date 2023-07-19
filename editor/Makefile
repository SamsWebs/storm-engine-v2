#!/bin/bash

BIN				= editor
BIN_DIR   		= $(PWD)/bin
TARGET 			= $(BIN_DIR)/$(BIN)
DATA_PREFIX   	= $(PWD)/assets/

SRCS	= $(wildcard vendor/imgui/*.cpp)
SRCS	+= $(wildcard src/**/*.cpp)
SRCS	+= $(wildcard src/*.cpp)
OBJS 	= $(SRCS:.cpp=.o)

all: clean $(TARGET)

include ../base.mk

run:
	$(TARGET)

$(TARGET) : $(OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $^ $(LIB) -lstormenginev2 -o $@
	/usr/bin/time -f "Compilation completed in : %E" $(TARGET)