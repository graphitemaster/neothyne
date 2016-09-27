#ifndef S_OBJECT_HDR
#define S_OBJECT_HDR

#include <string.h>

#include "s_instr.h"

#include "u_new.h"

namespace s {

struct Object;

struct Table {
    struct Entry {
        const char *m_name;
        Object *m_value;
        Entry *m_next;
    };

    Object *lookup(const char *key, bool *found);

private:
    Object **lookupReference(const char *key, Entry **first);

    friend struct GC;
    friend struct Object;
    Entry m_entry;
};

typedef Object *(*FunctionPointer)(Object *, Object *, Object *, Object **, size_t);

struct Object {
    static void *alloc(Object *context, size_t size);

    static Object *newObject(Object *context, Object *parent);
    static Object *newInt(Object *context, int value);
    static Object *newFloat(Object *context, float value);
    static Object *newBoolean(Object *context, bool value);
    static Object *newString(Object *content, const char *value);
    static Object *newArray(Object *context, Object **contents, size_t length);
    static Object *newFunction(Object *context, FunctionPointer function);
    static Object *newClosure(Object *context, UserFunction *function);

    static Object *instanceOf(Object *object, Object *prototype);

    static void free(Object *object);
    static void mark(Object *context, Object *object);

    Object *lookup(const char *key, bool *found);

    void setNormal(const char *key, Object *value);
    void setExisting(const char *key, Object *value);
    void setShadowing(const char *key, Object *value);

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
    char *m_value;
};

struct ArrayObject : Object {
    Object **m_contents;
    // Note: this would normally be size_t if this were not representing the
    //       size for the scripting language which only has signed integers
    int m_length;
};

}

#endif
