ROOT_DIR = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CC = g++
LIB = -L/usr/local/lib -Wl,-rpath=/usr/local/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer \
	-lz -ltinyxml2 -llua -ldl -lnfd $(shell pkg-config --libs gtk+-3.0)
INCLUDE = -I/usr/local/include -I$(ROOT_DIR)/vendor
CCFLAGS = -Wall -c -g -fPIC -std=c++17 -Wno-reorder -Wno-unused-parameter \
	-Wno-unused-variable -Wno-unused-function $(INCLUDE) $(shell pkg-config --cflags gtk+-3.0)

clean:
	rm -f $(BIN_DIR)/* && rm -f $(shell find $(ROOT_DIR) -name "*.o")

%.o: %.cpp
	$(CC) $(CCFLAGS) -o $@ $<
	
memcheck:
	valgrind --log-file=valgrind.output --leak-check=yes --leak-check=full --tool=memcheck -s $(TARGET)