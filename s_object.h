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
struct IntObject;

struct Field {
    // Name of the field
    const char *m_name;

    // The length of the field name
    size_t m_nameLength;

    // The field value
    void *m_value;

    void *m_aux;
};

struct Table {
    static Field *lookupAllocWithHash(Table *table,
                                      const char *key,
                                      size_t keyLength,
                                      size_t keyHash,
                                      Field **first);

    static Field *lookupAlloc(Table *table, const char *key, size_t keyLength, Field **first);

    static Field *lookupWithHash(Table *table, const char *key, size_t keyLength, size_t hash);

    static Field *lookup(Table *table, const char *key, size_t keyLength);

    // Array of fields
    Field *m_fields;

    // The amount of fields allocated to the table
    size_t m_fieldsNum;

    // The amount of fields stored
    size_t m_fieldsStored;

    // Bloom filter
    size_t m_bloom;

private:
    static Field *lookupWithHashInternalUnroll(Table *table, const char *key, size_t keyLength, size_t hash);
    static Field *lookupWithHashInternal(Table *table, const char *key, size_t keyLength, size_t hash);

    static Field *lookupAllocWithHashInternal(Table *table,
                                              const char *key,
                                              size_t keyLength,
                                              size_t keyHash,
                                              Field **first);
};

typedef void (*FunctionPointer)(State *state, Object *self, Object *function, Object **arguments, size_t count);

struct Object {
    static Object **lookupReferenceWithHash(Object *object, const char *key, size_t keyLength, size_t keyHash);
    static Object **lookupReference(Object *object, const char *key);
    static Object *lookupWithHash(Object *object, const char *key, size_t keyLength, size_t keyHash, bool *keyFound);
    static Object *lookup(Object *object, const char *key, bool *keyFound);

    static const char *setExisting(State *state, Object *object, const char *key, Object *value);
    static const char *setShadowing(State *state, Object *object, const char *key, Object *value, bool *set);
    static const char *setNormal(State *state, Object *object, const char *key, Object *value);
    static const char *setConstraint(State *state, Object *object, const char *key, size_t keyLength, Object *constraint);

    static void mark(State *state, Object *Object);
    static void free(Object *object);

    static Object *instanceOf(Object *object, Object *prototype);

    static void *allocate(State *state, size_t size);

    static Object *newObject(State *state, Object *parent);
    static Object *newInt(State *state, int value);
    static Object *newFloat(State *state, float value);
    static Object *newString(State *state, const char *value, size_t length);
    static Object *newBool(State *state, bool value);
    static Object *newBoolUncached(State *state, bool value);
    static Object *newArray(State *state, Object **data, IntObject *length);
    static Object *newFunction(State *state, FunctionPointer function);
    static Object *newMark(State *state);
    static Object *newClosure(State *state, Object *context, UserFunction *function);

    // Objects are basically tables
    Table m_table;

    // The parent object; all objects have parent objects except for the
    // Object object which is the root of the object hiearchy.
    Object *m_parent;

    // The size of the object
    size_t m_size;

    // Flags associated with this object
    ObjectFlags m_flags;

    // The previous object
    Object *m_prev;

    // The GC mark function to mark this object during GC mark phase
    void (*m_mark)(State *state, Object *object);
};

// Doubly linked list of GC roots
struct RootSet {
    // Array of objects in this GC root
    Object **m_objects;

    // The amount of objects in this GC root
    size_t m_count;

    // Pointer to the previous GC root
    RootSet *m_prev;

    // Pointer to the next GC root
    RootSet *m_next;
};

// Represents a call frame
struct CallFrame {
    // The function for this call frame
    UserFunction *m_function;

    // Array of slots allocated for this call frame
    Object **m_slots;

    // The amount of slots for this call frame
    size_t m_count;

    // References to slots in closed objects for this call frame
    Object ***m_fastSlots;

    // The amount of reference to slots in closed objects for this call frame
    size_t m_fastSlotsCount;

    // A GC root object for this call frame
    RootSet m_root;

    // The list of instructions for this call frame
    Instruction *m_instructions;
};

enum RunState {
    kTerminated,
    kRunning,
    kErrored
};

struct GCState {
    // The last GC root
    RootSet *m_tail;

    // The last allocated object
    Object *m_lastObjectAllocated;

    // The total number of objects allocated
    int m_numObjectsAllocated;

    // When to issue the next GC cycle
    int m_nextRun;

    // GC root of all permanent objects
    RootSet m_permanents;

    // How many times the GC has been issued disabledness since the last
    // cycle for some operation
    int m_disabledness;

    // Indicates that a GC cycle was meant to be issued but the GC was disabled
    // used to play catchup
    bool m_missed;
};

// The cache of all permanent objects
struct ValueCache {
    // [Bool]
    Object *m_boolFalse;
    Object *m_boolTrue;

    // [Int]
    Object *m_intZero;

    // Preallocated array of objects used to place function calls
    Object ***m_preallocatedArguments;

    // [Base]
    Object *m_intBase;
    Object *m_boolBase;
    Object *m_floatBase;
    Object *m_closureBase;
    Object *m_functionBase;
    Object *m_stringBase;
    Object *m_arrayBase;
};

struct ProfileState {
    // The last time we recorded a profile
    struct timespec m_lastTime;

    // The next VM cycle to record another profile
    int m_nextCheck;

    // Table encoding the amount of direct references to keys
    Table m_directTable;

    // Table encoding the amount of indirect reference to key
    Table m_indirectTable;

    static void dump(SourceRange source, ProfileState *profileState);
};

struct SharedState {
    // Garbage collector state
    GCState m_gcState;

    // Profiling state
    ProfileState m_profileState;

    // Cache of permanent objects
    ValueCache m_valueCache;

    // The amount of VM cycles
    int m_cycleCount;
};

struct State {
    // Parent VM state
    State *m_parent;

    // State shared across multiple substates
    SharedState *m_shared;

    // The stack for placing function calls
    CallFrame *m_stack;

    // The length of the stack
    size_t m_length;

    // The root object (literally Object's root)
    Object *m_root;

    // Resulting value object. For instance function call return
    Object *m_resultValue;

    // The running state of the VM
    RunState m_runState;

    // Contains a non empty string if the VM encountered an error
    u::string m_error;
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
