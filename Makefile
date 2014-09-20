CXX ?= g++
CXXFLAGS = \
	-std=c++11 \
	-Wall \
	-Wextra \
	-ffast-math \
	-fno-exceptions \
	-g3

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
	r_billboard.cpp \
	r_common.cpp \
	r_gbuffer.cpp \
	r_method.cpp \
	r_pipeline.cpp \
	r_quad.cpp \
	r_skybox.cpp \
	r_splash.cpp \
	r_texture.cpp \
	r_world.cpp

ENGINE_SOURCES = \
	engine.cpp \
	kdmap.cpp \
	kdtree.cpp \
	math.cpp \
	texture.cpp \
	u_file.cpp \
	u_zlib.cpp \
	u_sha512.cpp \
	$(RENDERER_SOURCES)

GAME_OBJECTS = $(GAME_SOURCES:.cpp=.o) $(ENGINE_SOURCES:.cpp=.o)
GAME_BIN = neothyne
GAME_DIR = game

all: $(GAME_BIN)

$(GAME_BIN): $(GAME_OBJECTS)
	$(CXX) $(ENGINE_LDFLAGS) $(GAME_OBJECTS) -o $@

install: $(GAME_BIN)
	strip $(GAME_BIN)
	mv $(GAME_BIN) $(GAME_DIR)/$(GAME_BIN)

uninstall:
	rm -f $(GAME_DIR)/$(GAME_BIN)

run: install
	cd $(GAME_DIR) && ./$(GAME_BIN)

.cpp.o:
	$(CXX) -c $(ENGINE_CXXFLAGS) $< -o $@

clean:
	rm -f $(GAME_OBJECTS)
	rm -f $(GAME_BIN)
