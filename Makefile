CC=g++
CFLAGS=-c -std=c++11 `sdl2-config --cflags` -Wall -Wextra -fomit-frame-pointer -fno-rtti -fno-exceptions -g3
LDFLAGS= `sdl2-config --libs` -lGL

SOURCES=kdmap.cpp kdtree.cpp main.cpp math.cpp renderer.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=neothyne

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS)
	rm -f $(EXECUTABLE)
