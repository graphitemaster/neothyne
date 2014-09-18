#ifndef ENGINE_HDR
#define ENGINE_HDR
#include <SDL2/SDL.h>
#include "util.h"

u::map<int, int> &neoKeyState(int key = 0, bool keyDown = false, bool keyUp = false);
void neoMouseDelta(int *deltaX, int *deltaY);
void neoSwap(void);
size_t neoWidth(void);
size_t neoHeight(void);
void neoToggleRelativeMouseMode(void);
void neoSetWindowTitle(const char *title);
void neoResize(size_t width, size_t height);
u::string neoPath(void);
[[noreturn]] void neoFatal(const char *fmt, ...);

#endif
