#include <string.h>

#include "s_object.h"
#include "s_runtime.h"
#include "s_gc.h"

#include "u_new.h"
#include "u_assert.h"
#include "u_log.h"

namespace s {

///! Table
Object **Table::lookupReference(Table *table, const char *key, Field** first) {
    if (!table->m_field.m_name) {
        if (first)
            *first = &table->m_field;
        return nullptr;
    }
    Field *field = &table->m_field;
    Field *prev;
    while (field) {
        if (key[0] == field->m_name[0] && strcmp(key, field->m_name) == 0)
            return &field->m_value;
        prev = field;
        field = field->m_next;
    }
    if (first) {
        field = (Field *)neoCalloc(sizeof *field, 1);
        prev->m_next = field;
        *first = field;
    }
    return nullptr;
}

Object *Table::lookup(Table *table, const char *key, bool *found) {
    Object **object = lookupReference(table, key, nullptr);
    if (object) {
        if (found)
            *found = true;
        return *object;
    }
    if (found)
        *found = false;
    return nullptr;
}

///! Object
Object *Object::lookup(Object *object, const char *key, bool *found) {
    while (object) {
        bool contains = false;
        Object *value = Table::lookup(&object->m_table, key, &contains);
        if (contains) {
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

void Object::mark(State *state, Object *object) {
    if (object) {
        // break cycles in the marking stage
        if (object->m_flags & kMarked)
            return;

        // set this object's marked flag
        object->m_flags |= kMarked;

        // if we're reachable then the parent is reachable
        mark(state, object->m_parent);

        // all fields of the object are reachable too
        Field *field = &object->m_table.m_field;
        while (field) {
            mark(state, field->m_value);
            field = field->m_next;
        }

        // run any custom mark functions if they exist
        if (object->m_mark)
            object->m_mark(state, object);
    }
}

void Object::free(Object *object) {
    Field *field = object->m_table.m_field.m_next;
    while (field) {
        Field *next = field->m_next;
        neoFree(field);
        field = next;
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
bool Object::setExisting(Object *object, const char *key, Object *value) {
    U_ASSERT(object);
    Object *current = object;
    while (current) {
        Object **search = Table::lookupReference(&current->m_table, key, nullptr);
        if (search) {
            U_ASSERT(!(current->m_flags & kImmutable));
            *search = value;
            return true;
        }
        current = current->m_parent;
    }
    return false;
}

// change a propery only if it exists somewhere in the prototype chain
bool Object::setShadowing(Object *object, const char *key, Object *value) {
    U_ASSERT(object);
    Object *current = object;
    while (current) {
        Object **search = Table::lookupReference(&current->m_table, key, nullptr);
        if (search) {
            setNormal(object, key, value);
            return true;
        }
        current = current->m_parent;
    }
    return false;
}

// set property
void Object::setNormal(Object *object, const char *key, Object *value) {
    U_ASSERT(object);
    Field *free = nullptr;
    Object **search = Table::lookupReference(&object->m_table, key, &free);
    if (search) {
        U_ASSERT(!(object->m_flags & kImmutable));
    } else {
        U_ASSERT(!(object->m_flags & kClosed));
        free->m_name = key;
        search = &free->m_value;
    }
    *search = value;
}

void *Object::allocate(State *state, size_t size) {
    if (state->m_count > state->m_capacity) {
        GC::run(state);
        // run gc after 20% growth or 10k elements
        state->m_capacity = size_t(state->m_count * 1.2) + 10000;
    }

    Object *result = (Object *)neoCalloc(size, 1);
    result->m_prev = state->m_last;
    result->m_size = size;
    state->m_last = result;
    state->m_count++;

    return result;
}

Object *Object::newObject(State *state, Object *parent) {
    Object *object = (Object *)allocate(state, sizeof *object);
    object->m_parent = parent;
    return object;
}

Object *Object::newInt(State *state, int value) {
    Object *intBase = lookup(state->m_root, "int", nullptr);
    IntObject *object = (IntObject *)allocate(state, sizeof *object);
    object->m_parent = intBase;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newFloat(State *state, float value) {
    Object *floatBase = lookup(state->m_root, "float", nullptr);
    FloatObject *object = (FloatObject *)allocate(state, sizeof *object);
    object->m_parent = floatBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newBool(State *state, bool value) {
    Object *boolBase = lookup(state->m_root, "bool", nullptr);
    BoolObject *object = (BoolObject *)allocate(state, sizeof *object);
    object->m_parent = boolBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newString(State *state, const char *value) {
    Object *stringBase = lookup(state->m_root, "string", nullptr);
    const size_t length = strlen(value);
    // string gets allocated as part of the object
    StringObject *object = (StringObject *)allocate(state, sizeof *object + length + 1);
    object->m_parent = stringBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = ((char *)object) + sizeof *object;
    strncpy(object->m_value, value, length + 1);
    return (Object *)object;
}

Object *Object::newArray(State *state, Object **contents, size_t length) {
    Object *arrayBase = lookup(state->m_root, "array", nullptr);
    ArrayObject *object = (ArrayObject *)allocate(state, sizeof *object);
    object->m_parent = arrayBase;
    object->m_contents = contents;
    object->m_length = length;
    GC::disable(state);
    setNormal((Object *)object, "length", newInt(state, length));
    GC::enable(state);
    return (Object *)object;
}

Object *Object::newFunction(State *state, FunctionPointer function) {
    Object *functionBase = lookup(state->m_root, "function", nullptr);
    FunctionObject *object = (FunctionObject*)allocate(state, sizeof *object);
    object->m_parent = functionBase;
    object->m_function = function;
    return (Object *)object;
}

}
