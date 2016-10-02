#ifndef S_RUNTIME_HDR
#define S_RUNTIME_HDR

#include <stddef.h>

namespace s {

struct State;
struct Object;

Object *createRoot(State *state);

}

#endif
