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
    friend struct GC;
    friend struct Object;
    Entry m_entry;
};

typedef Object *(*FunctionPointer)(Object *, Object *, Object *, Object **, size_t);

struct Object {
    static void *alloc(size_t size);

    static Object *newObject(Object *parent);
    static Object *newInt(Object *context, int value);
    static Object *newFloat(Object *context, float value);
    static Object *newBoolean(Object *context, bool value);
    static Object *newString(Object *content,const char *value);
    static Object *newFunction(Object *context, FunctionPointer function);
    static Object *newClosure(Object *context, UserFunction *function);

    void set(const char *key, Object *value);
    void setExisting(const char *key, Object *value);

    void mark();
    void free();

    enum {
        kClosed    = 1 << 0, // when set no additional properties can be added
        kImmutable = 1 << 1, // cannot be modified
        kMarked    = 1 << 2  // reachable in the GC mark phase
    };

    int m_flags;
    Object *m_parent;
    Object *m_prev;
    Table m_table;

    static Object *m_lastAllocated;
    static size_t m_numAllocated;
    static size_t m_nextGCRun;
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

struct FloatObject : Object {
    float m_value;
};

struct StringObject : Object {
    const char *m_value;
};

}

#endif
