#ifndef S_GC_HDR
#define S_GC_HDR

#include <stddef.h>

namespace s {

struct State;
struct Object;
struct RootSet;

struct GC {
    static void init(State *state);

    static void addPermanent(State *state, Object *object);

    static void addRoots(State *state, Object **objects, size_t count, RootSet *set);
    static void delRoots(State *state, RootSet *set);
    static void run(State *state);

    static void enable(State *state);
    static void disable(State *state);

private:
    static void mark(State *state);
    static void sweep(State *state);
};

}

#endif
