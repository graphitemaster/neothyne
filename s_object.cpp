#include <string.h>

#include "s_object.h"
#include "s_runtime.h"
#include "s_gc.h"

#include "u_new.h"
#include "u_assert.h"
#include "u_log.h"

namespace s {

// TODO(daleweiler): make this part of the virtual machine state
Object *Object::m_lastAllocated;
size_t Object::m_numAllocated;
size_t Object::m_nextGCRun = 10000;

///! Table
Object **Table::lookupReference(const char *key, Table::Entry **first) {
    if (!m_entry.m_name) {
        if (first)
            *first = &m_entry;
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

Object *Table::lookup(const char *key, bool *found) {
    Object **find = lookupReference(key, nullptr);
    if (!find) {
        if (found)
            *found = false;
        return nullptr;
    }
    if (found)
        *found = true;
    return *find;
}

Object *Object::lookup(const char *key, bool *found) {
    Object *object = this;
    while (object) {
        bool keyFound;
        Object *value = object->m_table.lookup(key, &keyFound);
        if (keyFound) {
            if (found)
                *found = true;
            return value;
        }
        object = object->m_parent;
    }
    if (found)
        *found = false;
    return nullptr;
}

///! Object
void Object::mark(Object *context, Object *object) {
    // break cycles
    if (!object || object->m_flags & kMarked)
        return;
    object->m_flags |= kMarked;

    // mark parent object
    mark(context, object->m_parent);

    // mark all entries in the table
    Table::Entry *entry = &object->m_table.m_entry;
    while (entry) {
        mark(context, entry->m_value);
        entry = entry->m_next;
    }

    // check for mark function and call it if exists
    bool markFunctionFound;
    Object *markFunction = object->lookup("mark", &markFunctionFound);
    if (markFunctionFound) {
        FunctionObject *markFunctionObject = (FunctionObject *)markFunction;
        markFunctionObject->m_function(context, object, markFunction, nullptr, 0);
    }
}

void Object::free(Object *object) {
    Table::Entry *entry = object->m_table.m_entry.m_next;
    while (entry) {
        Table::Entry *next = entry->m_next;
        neoFree(entry);
        entry = next;
    }
    neoFree(object);
}

Object *Object::instanceOf(Object *object, Object *prototype) {
    // search the prototype chain to see if 'object' is an instance of 'prototype'
    while (object) {
        if (object->m_parent == prototype)
            return object;
        object = object->m_parent;
    }
    return nullptr;
}


// changes a propery in place
void Object::setExisting(const char *key, Object *value) {
    Object *current = this;
    while (current) {
        Object **find = current->m_table.lookupReference(key, nullptr);
        if (find) {
            U_ASSERT(!(m_flags & kImmutable));
            *find = value;
            return;
        }
        current = current->m_parent;
    }
    U_UNREACHABLE();
}

// change a propery only if it exists somewhere in the prototype chain
void Object::setShadowing(const char *key, Object *value) {
    Object *current = this;
    while (current) {
        Object **find = current->m_table.lookupReference(key, nullptr);
        if (find) {
            // Create it in the object
            setNormal(key, value);
            return;
        }
        current = current->m_parent;
    }

    U_UNREACHABLE();
}

// set property
void Object::setNormal(const char *key, Object *value) {
    Table::Entry *free = nullptr;
    Object **find = m_table.lookupReference(key, &free);
    if (find) {
        U_ASSERT(!(m_flags & kImmutable));
    } else {
        U_ASSERT(!(m_flags & kClosed));
        free->m_name = key;
        find = &free->m_value;
    }
    *find = value;
}

void *Object::alloc(Object *context, size_t size) {
    if (m_numAllocated > m_nextGCRun) {
        GarbageCollector::run(context);
        // TODO(daleweiler): cleanup in vm state ?
        m_nextGCRun = m_numAllocated * 1.2f;
    }

    Object *result = (Object *)memset(neoMalloc(size), 0, size);
    result->m_prev = m_lastAllocated;
    m_lastAllocated = result;
    m_numAllocated++;
    return result;
}

Object *Object::newObject(Object *context, Object *parent) {
    Object *object = (Object *)alloc(context, sizeof(Object));
    object->m_parent = parent;
    return object;
}

Object *Object::newInt(Object *context, int value) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *intBase = root->lookup("int", nullptr);
    auto *object = (IntObject *)alloc(context, sizeof(IntObject));
    object->m_parent = intBase;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newFloat(Object *context, float value) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *floatBase = root->lookup("float", nullptr);
    auto *object = (FloatObject *)alloc(context, sizeof(FloatObject));
    object->m_parent = floatBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newBoolean(Object *context, bool value) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *boolBase = root->lookup("bool", nullptr);
    auto *object = (BooleanObject *)alloc(context, sizeof(BooleanObject));
    object->m_parent = boolBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newString(Object *context, const char *value) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *stringBase = root->lookup("string", nullptr);
    size_t length = strlen(value);
    // string gets allocated as part of the object
    auto *object = (StringObject *)alloc(context, sizeof(StringObject) + length + 1);
    object->m_parent = stringBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = ((char *)object) + sizeof(StringObject);
    strncpy(object->m_value, value, length + 1);
    return (Object *)object;
}

Object *Object::newArray(Object *context, Object **contents, size_t length) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *arrayBase = root->lookup("array", nullptr);
    auto *object = (ArrayObject *)alloc(context, sizeof(ArrayObject));
    object->m_parent = arrayBase;
    object->m_contents = contents;
    object->m_length = length;
    ((Object *)object)->setNormal("length", newInt(context, length));
    return (Object *)object;
}

Object *Object::newFunction(Object *context, FunctionPointer function) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *functionBase = root->lookup("function", nullptr);
    auto *object = (FunctionObject*)alloc(context, sizeof(FunctionObject));
    object->m_parent = functionBase;
    object->m_function = function;
    return (Object *)object;
}

Object *Object::newClosure(Object *context, UserFunction *function) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *closureBase = root->lookup("closure", nullptr);
    auto *object = (ClosureObject*)alloc(context, sizeof(ClosureObject));
    object->m_parent = closureBase;
    if (function->m_isMethod)
        object->m_function = methodHandler;
    else
        object->m_function = functionHandler;
    object->m_context = context;
    object->m_userFunction = *function;
    return (Object *)object;
}

}
