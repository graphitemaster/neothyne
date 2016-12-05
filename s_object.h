#ifndef S_OBJECT_HDR
#define S_OBJECT_HDR

#include <string.h>
#include <time.h>

#include "s_instr.h"
#include "s_util.h"

#include "u_new.h"
#include "u_string.h"

namespace s {

enum {
    kNone      = 1 << 1,
    kClosed    = 1 << 2,
    kImmutable = 1 << 3,
    kNoInherit = 1 << 4,
    kMarked    = 1 << 5
};
using ObjectFlags = int;

struct Object;
struct Field;
struct Table;
struct RootSet;
struct CallFrame;
struct State;

struct Field {
    const char *m_name;
    size_t m_nameLength;
    void *m_value;
};

struct Table {
    static void **lookupReference(Table *table, const char *key, size_t keyLength);
    static void **lookupReference(Table *table, const char *key, size_t keyLength, void ***first);
    static void **lookupReference(Table *table, size_t keyHash, const char *key, size_t keyLength, void ***first);

    static void *lookup(Table *table, const char *key, size_t keyLength, bool *found);

    Field *m_fields;
    size_t m_fieldsNum;
    size_t m_fieldsStored;
};

typedef void (*FunctionPointer)(State *state, Object *self, Object *function, Object **arguments, size_t count);

struct Object {

    static Object *lookup(Object *object, const char *key, bool *found);

    static bool setExisting(Object *object, const char *key, Object *value);
    static bool setShadowing(Object *object, const char *key, Object *value);
    static void setNormal(Object *object, const char *key, Object *value);

    static void mark(State *state, Object *Object);
    static void free(Object *object);

    static Object *instanceOf(Object *object, Object *prototype);

    static void *allocate(State *state, size_t size);

    static Object *newObject(State *state, Object *parent);
    static Object *newInt(State *state, int value);
    static Object *newFloat(State *state, float value);
    static Object *newString(State *state, const char *value);
    static Object *newBool(State *state, bool value);
    static Object *newArray(State *state, Object **data, size_t count);
    static Object *newFunction(State *state, FunctionPointer function);
    static Object *newMark(State *state);
    static Object *newClosure(Object *context, UserFunction *function);

    Table m_table;
    Object *m_parent;
    size_t m_size;
    ObjectFlags m_flags;
    Object *m_prev;
    void (*m_mark)(State *state, Object *object);
};

struct RootSet {
    Object **m_objects;
    size_t m_count;
    RootSet *m_prev;
    RootSet *m_next;
};

struct CallFrame {
    UserFunction *m_function;
    Object *m_context;
    Object **m_slots;
    size_t m_count;
    RootSet m_root;
    Instruction *m_instructions;
};

enum RunState {
    kTerminated,
    kRunning,
    kErrored
};

struct GCState {
    RootSet *m_tail;
    size_t m_disabledCount;
};

struct ProfileState {
    struct timespec m_lastTime;
    int m_nextCheck;
    Table m_directTable;
    Table m_indirectTable;
    static void dump(SourceRange source, ProfileState *profileState);
};

struct State {
    State *m_parent;
    CallFrame *m_stack;
    size_t m_length;
    Object *m_root;
    Object *m_result;
    RunState m_runState;
    u::string m_error;
    GCState *m_gc;
    Object *m_last;
    size_t m_count;
    size_t m_capacity;
    ProfileState *m_profileState;
};

struct FunctionObject : Object {
    FunctionPointer m_function;
};

struct ClosureObject : FunctionObject {
    Object *m_context;
    UserFunction m_closure;
};

struct IntObject : Object {
    int m_value;
};

struct BoolObject : Object {
    bool m_value;
};

struct FloatObject : Object {
    float m_value;
};

struct StringObject : Object {
    char *m_value;
};

struct ArrayObject : Object {
    Object **m_contents;
    int m_length;
};

}

#endif
