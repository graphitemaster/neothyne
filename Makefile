CXX ?= g++
CFLAGS=-c -std=c++11 `sdl2-config --cflags` -Wall -Wextra -fomit-frame-pointer -fno-exceptions -O3
LDFLAGS= `sdl2-config --libs` -lSDL2_image -lGL -lz

SOURCES=kdmap.cpp kdtree.cpp main.cpp math.cpp renderer.cpp texture.cpp client.cpp util.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=neothyne

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CXX) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS)
	rm -f $(EXECUTABLE)
