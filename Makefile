CXX ?= g++
CFLAGS=-c -std=c++11 `sdl2-config --cflags` -Wall -Wextra -ffast-math -fomit-frame-pointer -fno-exceptions -O2
LDFLAGS= `sdl2-config --libs` -lGL

GAMESOURCES=client.cpp main.cpp
RENDERSOURCES=r_common.cpp r_pipeline.cpp r_texture.cpp renderer.cpp
SOURCES=engine.cpp kdmap.cpp kdtree.cpp math.cpp texture.cpp util.cpp $(RENDERSOURCES) $(GAMESOURCES)
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
