#!/bin/bash

include ../examples.mk

BIN	        = tanks
BIN_DIR     = $(PWD)/bin
TARGET 		= $(BIN_DIR)/$(BIN)
TESTTARGET  = $(BIN_DIR)/test-$(BIN)
DATA_PREFIX = $(PWD)/assets/

LIB += -lstorm-engine2
CCFLAGS += -DDATA_PREFIX=\"$(DATA_PREFIX)\"

SRCS	= $(wildcard src/**/*.cpp)
SRCS	+= $(wildcard src/*.cpp)
SRCS	+= ./main.cpp

TESTRCS  = $(wildcard specs/*.cpp)
TESTRCS  += $(wildcard src/*.cpp)
TESTOBJS  = $(TESTRCS:.cpp=.o)

$(TARGET) : $(OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $^ $(LIB) -o $@