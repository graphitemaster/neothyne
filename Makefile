CXX ?= g++
CFLAGS=-c -std=c++11 `sdl2-config --cflags` -Wall -Wextra -ffast-math -fomit-frame-pointer -fno-exceptions -O2
LDFLAGS= `sdl2-config --libs` -lGL

SOURCES=kdmap.cpp kdtree.cpp main.cpp math.cpp r_common.cpp renderer.cpp texture.cpp client.cpp util.cpp
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
