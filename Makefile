.SUFFIXES: .lo

CXX ?= g++
CXXFLAGS = \
	-std=c++11 \
	-Wall \
	-Wextra \
	-ffast-math \
	-fomit-frame-pointer \
	-fno-exceptions \
	-O2

ENGINE_CXXFLAGS = \
	$(CXXFLAGS) \
	`sdl2-config --cflags` \
	-DDEBUG_GL

ENGINE_LDFLAGS = \
	`sdl2-config --libs` \
	-lGL

GAME_SOURCES = \
	client.cpp \
	main.cpp

RENDERER_SOURCES = \
	r_common.cpp \
	r_pipeline.cpp \
	r_texture.cpp \
	renderer.cpp

ENGINE_SOURCES = \
	engine.cpp \
	kdmap.cpp \
	kdtree.cpp \
	math.cpp \
	texture.cpp \
	util.cpp \
	$(RENDERER_SOURCES)

RGEN_SOURCES = \
	r_generator.cpp \
	util.cpp

GAME_OBJECTS = $(GAME_SOURCES:.cpp=.o) $(ENGINE_SOURCES:.cpp=.o)
RGEN_OBJECTS = $(RGEN_SOURCES:.cpp=.lo)

GAME_BIN = neothyne
RGEN_BIN = rgen

all: $(GAME_BIN)

$(GAME_BIN): $(GAME_OBJECTS)
	$(CXX) $(ENGINE_LDFLAGS) $(GAME_OBJECTS) -o $@

$(RGEN_BIN): $(RGEN_OBJECTS)
	$(CXX) $(RGEN_OBJECTS) -o $@

.cpp.o:
	$(CXX) -c $(ENGINE_CXXFLAGS) $< -o $@

.cpp.lo:
	$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -f $(GAME_OBJECTS)
	rm -f $(RGEN_OBJECTS)
	rm -f $(GAME_BIN)
	rm -f $(RGEN_BIN)
