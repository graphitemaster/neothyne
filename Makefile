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
	-fno-exceptions \
	-fno-rtti \
	-I. \
	-DDEBUG_GL \
	-DDXT_COMPRESSOR \
	-Os

ifneq (, $(findstring -g, $(CXXFLAGS)))
	STRIP = true
else
	STRIP = strip
endif

ENGINE_CXXFLAGS = \
	$(CXXFLAGS) \
	`sdl2-config --cflags`

ENGINE_LDFLAGS = \
	-lm \
	`sdl2-config --libs`

all: $(GAME_BIN)

$(GAME_BIN): $(GAME_OBJECTS)
	$(CXX) $(GAME_OBJECTS) $(ENGINE_LDFLAGS) -o $@
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
