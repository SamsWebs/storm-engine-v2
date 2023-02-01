#!/bin/sh

ROOT_DIR = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CC = g++
LIB = -L/usr/local/lib -Wl,-rpath=/usr/local/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lz -ltinyxml2
INCLUDE = -isystem -I/usr/local/include -I$(ROOT_DIR)/vendor -I$(ROOT_DIR)/common
CCFLAGS = -Wall -c -g -std=c++17 -DDATA_PREFIX=\"$(DATA_PREFIX)\" \
	-Wno-reorder -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function  $(INCLUDE) 

clean:
	rm -f $(BIN_DIR)/* && rm -f $(shell find $(ROOT_DIR) -name "*.o")

.cpp.o: 
	$(CC) $(CCFLAGS) $< -o $@
	
memcheck:
	valgrind --log-file=valgrind.output --leak-check=yes --tool=memcheck $(TARGET)