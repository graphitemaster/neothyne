include include.mk

# Dependency management
DEP_DIR = .deps
DEP_FLAGS = -MT $@ -MMD -MP -MF $(DEP_DIR)/$*.p
DEP_COPY = @mv -f $(DEP_DIR)/$*.p $(DEP_DIR)/$*.d

# Ensure dependency directory exists
$(shell mkdir -p $(DEP_DIR)/$(GAME_DIR) >/dev/null)

CC ?= clang
CXX = $(CC)

GAME_BIN = neothyne

CXXFLAGS = \
	-std=c++11 \
	-march=native \
	-Wall \
	-Wextra \
	-ffast-math \
	-fno-exceptions \
	-fno-rtti \
	-fno-stack-protector \
	-I. \
	-D_FORTIFY_SOURCE=0\
	-DDEBUG_GL \
	-DDXT_COMPRESSOR \
	-O3

# clang does not have -Wstict-aliasing=3
ifeq (, $(findstring clang, $(CC)))
	CXXFLAGS += -Wstrict-aliasing=3
endif

ifneq (, $(findstring -g, $(CXXFLAGS)))
	STRIP = true
else
	STRIP = strip
endif

ENGINE_CXXFLAGS = \
	$(CXXFLAGS) \
	`sdl2-config --cflags`

ENGINE_LDFLAGS = \
	`sdl2-config --libs`

all: $(GAME_BIN)

$(GAME_BIN): $(GAME_OBJECTS)
	$(CXX) $(CXXFLAGS) $(GAME_OBJECTS) $(ENGINE_LDFLAGS) -o $@
	$(STRIP) $@

.cpp.o: $(DEP_DIR)/%.d
	$(CXX) $(DEP_FLAGS) $(ENGINE_CXXFLAGS) -c $< -o $@
	$(DEP_COPY)

# Some rules to prevent Make from deleting these
$(DEP_DIR)/*.d: ;
$(DEP_DIR)/$(GAME_DIR)*.d: ;
.PRECIOUS: $(DEP_DIR)/%.d $(DEP_DIR)/$(GAME_DIR)%.d

clean:
	rm -f $(GAME_OBJECTS)
	rm -rf $(DEP_DIR)
	rm -f $(GAME_BIN)

# Include dependencies
-include $(patsubst %,$(DEP_DIR)/%.d,$(basename $(GAME_OBJECTS)))
