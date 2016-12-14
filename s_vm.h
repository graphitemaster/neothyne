#ifndef S_VM_HDR
#define S_VM_HDR
#include <stddef.h>

namespace s {

struct CallFrame;
struct State;
struct Object;
struct Instruction;
struct UserFunction;

struct VMState {
    Object *m_root;
    CallFrame *m_cf;
    Instruction *m_instr;
    State *m_restState;
};

struct VMFnWrap;
typedef VMFnWrap (*VMInstrFn)(VMState *state);
struct VMFnWrap {
    VMInstrFn self;
};

struct VM {
    // Call frame management
    static CallFrame *addFrame(State *state, size_t slots);
    static void delFrame(State *state);

    // Execute the V
    static void run(State *state);

    // Execution handlers
    static void functionHandler(State *state, Object *self, Object *function, Object **arguments, size_t count);
    static void methodHandler(State *state, Object *self, Object *function, Object **arguments, size_t count);

    static void error(State *state, const char *fmt, ...);

    static void callFunction(State *state, Object *context, UserFunction *function, Object **arguments, size_t count);

private:
    // Profiling
    static void recordProfile(State *state);
    static long long getClockDifference(struct timespec *targetClock,
                                        struct timespec *compareClock);

    // Execute one cycle of the VM
    static void step(State *state);
};

#define VM_ASSERT(CONDITION, ...) \
    do { \
        if (!(CONDITION)) { \
            VM::error(state, __VA_ARGS__); \
            return; \
        } \
    } while(0)

#define VM_ASSERT_TYPE(CONDITION, TYPE) \
    VM_ASSERT(CONDITION, "wrong type: expected " TYPE)

#define VM_ASSERT_ARITY(EXPECTED, GOT) \
    VM_ASSERT((GOT) == (EXPECTED), "expected %zu arguments, got %zu", (EXPECTED), (GOT))

}

#endif
