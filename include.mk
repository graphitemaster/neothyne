GAME_SOURCES = \
	game/menu.cpp \
	game/client.cpp \
	game/main.cpp \
	game/edit.cpp \
	game/world.cpp

MATH_SOURCES = \
	m_half.cpp \
	m_mat.cpp \
	m_quat.cpp \
	m_vec.cpp \
	m_plane.cpp \
	m_trig.cpp

RENDERER_SOURCES = \
	r_aa.cpp \
	r_billboard.cpp \
	r_common.cpp \
	r_composite.cpp \
	r_gbuffer.cpp \
	r_model.cpp \
	r_method.cpp \
	r_pipeline.cpp \
	r_geom.cpp \
	r_grader.cpp \
	r_shadow.cpp \
	r_skybox.cpp \
	r_ssao.cpp \
	r_stats.cpp \
	r_texture.cpp \
	r_vignette.cpp \
	r_world.cpp \
	r_gui.cpp \
	r_light.cpp \
	r_particles.cpp

AUDIO_SOURCES = \
	a_fader.cpp \
	a_filter.cpp \
	a_lane.cpp \
	a_system.cpp \
	a_wav.cpp

UTIL_SOURCES = \
	u_assert.cpp \
	u_file.cpp \
	u_misc.cpp \
	u_new.cpp \
	u_hash.cpp \
	u_log.cpp \
	u_string.cpp \
	u_zip.cpp \
	u_zlib.cpp

CONSOLE_SOURCES = \
	c_complete.cpp \
	c_config.cpp \
	c_console.cpp \
	c_variable.cpp

SCRIPTING_SOURCES = \
	s_gc.cpp \
	s_gen.cpp \
	s_instr.cpp \
	s_object.cpp \
	s_parser.cpp \
	s_runtime.cpp \
	s_util.cpp \
	s_vm.cpp \
	s_memory.cpp \
	s_optimize.cpp

ENGINE_SOURCES = \
	engine.cpp \
	kdmap.cpp \
	kdtree.cpp \
	mesh.cpp \
	model.cpp \
	texture.cpp \
	gui.cpp \
	grader.cpp \
	$(AUDIO_SOURCES) \
	$(UTIL_SOURCES) \
	$(CONSOLE_SOURCES) \
	$(SCRIPTING_SOURCES) \
	$(MATH_SOURCES) \
	$(RENDERER_SOURCES)

GAME_OBJECTS = \
	$(GAME_SOURCES:.cpp=.o) \
	$(ENGINE_SOURCES:.cpp=.o)

GAME_DIR = game
