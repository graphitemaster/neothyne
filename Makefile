CC=g++
CFLAGS=-c -std=c++11 -Wall -Wextra -fomit-frame-pointer -fno-rtti -fno-exceptions
LDFLAGS=
SOURCES=kdmap.cpp kdtree.cpp main.cpp math.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=neothyne

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@
