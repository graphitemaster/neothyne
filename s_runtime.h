#ifndef S_RUNTIME_HDR
#define S_RUNTIME_HDR

namespace s {

struct State;
struct Object;

Object *createRoot(State *state);
const char *getTypeString(State *state, Object *object);

}

#endif
