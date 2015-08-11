include include.mk

CC ?= clang
CXX = $(CC)

GAME_BIN = neothyne

CXXFLAGS = \
	-std=c++11 \
	-Wall \
	-Wextra \
	-ffast-math \
	-fno-exceptions \
	-fno-rtti \
	-I. \
	-DDEBUG_GL \
	-DDXT_COMPRESSOR \
	-O3

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

.cpp.o:
	$(CXX) -MD -c $(ENGINE_CXXFLAGS) $< -o $@
	@cp $*.d $*.P; \
		sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
			-e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P; \
		rm -f $*.d

clean:
	rm -f $(GAME_OBJECTS) $(GAME_OBJECTS:.o=.P)
	rm -f $(GAME_BIN)

-include *.P
