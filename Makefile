#!/bin/bash

BIN				= tests
BIN_DIR   		= ./bin
TARGET 			= $(BIN_DIR)/$(BIN)

all: clean test-target run-test

include ./base.mk

test: test-target

TESTRCS  = $(wildcard specs/*.cpp)
TESTRCS  += $(wildcard common/*.cpp)
TESTOBJS  = $(TESTRCS:.cpp=.o)

run-test:
	$(TARGET)

test-target: $(TESTOBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $^ $(LIB) -pthread -o $(TARGET)