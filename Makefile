#!/bin/bash

include ./base.mk

BIN_DIR     = ./bin
TESTTARGET  = $(BIN_DIR)/test-lstorm-eng2

LIB = -L/usr/local/lib -Wl,-rpath=/usr/local/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lz -ltinyxml
INCLUDE = -isystem -I/usr/local/include
CCFLAGS = -Wall -c -g -std=c++17 \
	-Wno-reorder -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function  $(INCLUDE) 

SRCS	= $(wildcard common/*.cpp)
SRCS	+= $(wildcard vendor/**/*.cpp)

TESTRCS  = $(wildcard specs/*.cpp)
TESTRCS  += $(wildcard common/*.cpp)
TESTOBJS  = $(TESTRCS:.cpp=.o)