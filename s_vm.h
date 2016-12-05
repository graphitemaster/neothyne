#ifndef S_VM_HDR
#define S_VM_HDR

namespace s {

struct CallFrame;
struct State;
struct Object;

struct VM {
    static CallFrame *addFrame(State *state, size_t slots);
    static void delFrame(State *state);
    static void run(State *state);

    static void callFunction(State *state, Object *context, UserFunction *function, Object **arguments, size_t count);

    static void functionHandler(State *state, Object *self, Object *function, Object **arguments, size_t count);
    static void methodHandler(State *state, Object *self, Object *function, Object **arguments, size_t count);

    static void error(State *state, const char *fmt, ...);

private:
    static void recordProfile(State *state);

    static long long getClockDifference(struct timespec *targetClock,
                                        struct timespec *compareClock);

    static void step(State *state);
};

#define VM_ASSERT(CONDITION, ...) \
    do { \
        if (!(CONDITION)) { \
            VM::error(state, __VA_ARGS__); \
            return; \
        } \
    } while(0)

}

#endif
