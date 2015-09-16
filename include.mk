GAME_SOURCES = \
	game/menu.cpp \
	game/client.cpp \
	game/main.cpp \
	game/edit.cpp

MATH_SOURCES = \
	m_half.cpp \
	m_mat.cpp \
	m_quat.cpp \
	m_vec.cpp \
	m_plane.cpp

RENDERER_SOURCES = \
	r_aa.cpp \
	r_billboard.cpp \
	r_common.cpp \
	r_gbuffer.cpp \
	r_model.cpp \
	r_method.cpp \
	r_pipeline.cpp \
	r_geom.cpp \
	r_grader.cpp \
	r_shadow.cpp \
	r_skybox.cpp \
	r_ssao.cpp \
	r_texture.cpp \
	r_world.cpp \
	r_gui.cpp \
	r_light.cpp \
	r_particles.cpp

UTIL_SOURCES = \
	u_file.cpp \
	u_misc.cpp \
	u_new.cpp \
	u_sha512.cpp \
	u_string.cpp \
	u_zlib.cpp \

ENGINE_SOURCES = \
	engine.cpp \
	kdmap.cpp \
	kdtree.cpp \
	world.cpp \
	mesh.cpp \
	model.cpp \
	texture.cpp \
	cvar.cpp \
	gui.cpp \
	grader.cpp \
	$(UTIL_SOURCES) \
	$(MATH_SOURCES) \
	$(RENDERER_SOURCES)

GAME_OBJECTS = \
	$(GAME_SOURCES:.cpp=.o) \
	$(ENGINE_SOURCES:.cpp=.o)

GAME_DIR = game
