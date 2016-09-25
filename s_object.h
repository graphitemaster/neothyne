#ifndef S_OBJECT_HDR
#define S_OBJECT_HDR

#include <string.h>

#include "s_instr.h"

#include "u_new.h"

namespace s {

template <typename T>
static inline T *allocate(size_t count = 1) {
    return (T *)memset((void *)neoMalloc(sizeof(T) * count), 0, sizeof(T) * count);
}

struct Object;

struct Table {
    struct Entry {
        const char *m_name;
        Object *m_value;
        Entry *m_next;
    };

    Object **lookup(const char *key, Entry **first);
    Object *lookup(const char *key);
    void set(const char *key, Object *value);

private:
    Entry m_entry;
};

typedef Object *(*FunctionPointer)(Object *, Object *, Object **, size_t);

struct Object {
    static Object *newObject(Object *parent);
    static Object *newInt(Object *context, int value);
    static Object *newBoolean(Object *context, bool value);
    static Object *newFunction(Object *context, FunctionPointer function);
    static Object *newClosure(Object *context, UserFunction *function);

    void set(const char *key, Object *value);

    enum {
        kClosed = 1 << 0 // when set no additional properties can be added
    };

    int m_flags;
    Object *m_parent;
    Table m_table;
};

struct FunctionObject : Object {
    FunctionPointer m_function;
};

struct ClosureObject : FunctionObject {
    Object *m_context;
    UserFunction m_userFunction;
};

struct IntObject : Object {
    int m_value;
};

struct BooleanObject : Object {
    bool m_value;
};

}

#endif
