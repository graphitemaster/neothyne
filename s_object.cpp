#include <string.h>

#include "s_object.h"
#include "s_runtime.h"
#include "s_gc.h"

#include "u_new.h"
#include "u_assert.h"
#include "u_log.h"

namespace s {

///! Table
Object **Table::lookup(const char *key, Table::Entry **first) {
    if (!m_entry.m_name) {
        if (first) *first = &m_entry;
        return nullptr;
    }
    Entry *entry = &m_entry;
    Entry *prev;
    while (entry) {
        if (strcmp(key, entry->m_name) == 0)
            return &entry->m_value;
        prev = entry;
        entry = entry->m_next;
    }
    if (first) {
        Entry *next = allocate<Entry>();
        prev->m_next = next;
        *first = next;
    }
    return nullptr;
};

Object *Table::lookup(const char *key) {
    Object **find = lookup(key, nullptr);
    return find ? *find : nullptr;
}

void Table::set(const char *key, Object *value) {
    Entry *free;
    Object **find = lookup(key, &free);
    if (!find) {
        free->m_name = key;
        find = &free->m_value;
    }
    *find = value;
}

///! Object
Object *Object::m_lastAllocated;
size_t Object::m_numAllocated;
size_t Object::m_nextGCRun = 10000;

// mark an object for garbage collection
void Object::mark() {
    // break cycles
    if (m_flags & kMarked)
        return;

    m_flags |= kMarked;

    // mark the parent
    if (m_parent)
        m_parent->mark();

    // mark table entries
    Table::Entry *entry = &m_table.m_entry;
    while (entry) {
        if (entry->m_value)
            entry->m_value->mark();
        entry = entry->m_next;
    }

    // mark the closures
    Object *markFunction = m_table.lookup("mark");
    if (markFunction) {
        FunctionObject *markFunctionObject = (FunctionObject *)markFunction;
        markFunctionObject->m_function(nullptr, this, markFunction, nullptr, 0);
    }
}

void Object::free() {
    Table::Entry *entry = m_table.m_entry.m_next;
    while (entry) {
        Table::Entry *next = entry->m_next;
        neoFree(entry);
        entry = next;
    }
    neoFree(this);
}

void *Object::alloc(size_t size) {
    if (m_numAllocated > m_nextGCRun) {
        GC::run();
        // run after 20% growth
        m_nextGCRun = m_numAllocated * 1.2f;
    }

    Object *result = (Object *)memset(neoMalloc(size), 0, size);
    result->m_prev = m_lastAllocated;
    m_lastAllocated = result;
    m_numAllocated++;
    return result;
}

Object *Object::newObject(Object *parent) {
    Object *object = (Object *)alloc(sizeof(Object));
    object->m_parent = parent;
    return object;
}

Object *Object::newInt(Object *context, int value) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *intBase = root->m_table.lookup("int");
    auto *object = (IntObject *)alloc(sizeof(IntObject));
    object->m_parent = intBase;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newFloat(Object *context, float value) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *floatBase = root->m_table.lookup("float");
    auto *object = (FloatObject *)alloc(sizeof(FloatObject));
    object->m_parent = floatBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newBoolean(Object *context, bool value) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *booleanBase = root->m_table.lookup("boolean");
    auto *object = (BooleanObject *)alloc(sizeof(BooleanObject));
    object->m_parent = booleanBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newString(Object *context, const char *value) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *stringBase = root->m_table.lookup("string");
    size_t length = strlen(value);
    // string gets allocated as part of the object
    auto *object = (StringObject *)alloc(sizeof(StringObject) + length + 1);
    object->m_parent = stringBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = ((char *)object) + sizeof(StringObject);
    strncpy(object->m_value, value, length + 1);
    return (Object *)object;
}

Object *Object::newFunction(Object *context, FunctionPointer function) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *functionBase = root->m_table.lookup("function");
    auto *object = (FunctionObject*)alloc(sizeof(FunctionObject));
    object->m_parent = functionBase;
    object->m_function = function;
    return (Object *)object;
}

Object *Object::newClosure(Object *context, UserFunction *function) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *closureBase = root->m_table.lookup("closure");
    auto *object = (ClosureObject*)alloc(sizeof(ClosureObject));
    object->m_parent = closureBase;
    object->m_function = functionHandler;
    object->m_context = context;
    object->m_userFunction = *function;
    return (Object *)object;
}

void Object::set(const char *key, Object *value) {
    Table::Entry *free = nullptr;
    Object **find = m_table.lookup(key, &free);
    if (find) {
        U_ASSERT(!(m_flags & kImmutable));
    } else {
        U_ASSERT(!(m_flags & kClosed));
        free->m_name = key;
        find = &free->m_value;
    }
    *find = value;
}

// changes a propery in place
void Object::setExisting(const char *key, Object *value) {
    Object *current = this;
    while (current) {
        Object **find = current->m_table.lookup(key, nullptr);
        if (find) {
            U_ASSERT(!(m_flags & kImmutable));
            *find = value;
            return;
        }
        current = current->m_parent;
    }
    U_UNREACHABLE();
}

}
