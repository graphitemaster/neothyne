CXX = $(CC)
CXXFLAGS = \
	-std=c++11 \
	-Wall \
	-Wextra \
	-ffast-math \
	-fno-exceptions \
	-fno-rtti \
	-DDEBUG_GL \
	-O3

ENGINE_CXXFLAGS = \
	$(CXXFLAGS) \
	`sdl2-config --cflags`

ENGINE_LDFLAGS = \
	-lm \
	`sdl2-config --libs`

GAME_SOURCES = \
	client.cpp \
	main.cpp

MATH_SOURCES = \
	m_mat4.cpp \
	m_quat.cpp \
	m_vec3.cpp

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
	texture.cpp \
	c_var.cpp \
	u_new.cpp \
	u_file.cpp \
	u_zlib.cpp \
	u_sha512.cpp \
	u_string.cpp \
	$(MATH_SOURCES) \
	$(RENDERER_SOURCES)

GAME_OBJECTS = $(GAME_SOURCES:.cpp=.o) $(ENGINE_SOURCES:.cpp=.o)
GAME_BIN = neothyne
GAME_DIR = game

all: $(GAME_BIN)

$(GAME_BIN): $(GAME_OBJECTS)
	$(CXX) $(GAME_OBJECTS) $(ENGINE_LDFLAGS) -o $@

.cpp.o:
	$(CXX) -c $(ENGINE_CXXFLAGS) $< -o $@

clean:
	rm -f $(GAME_OBJECTS)
	rm -f $(GAME_BIN)
