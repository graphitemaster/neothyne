#include <string.h>

#include "s_object.h"
#include "s_runtime.h"
#include "s_gc.h"

#include "u_new.h"
#include "u_assert.h"
#include "u_log.h"

namespace s {

///! Table
void **Table::lookupReferenceWithHashInternal(Table *table, const char *key, size_t keyLength, size_t hash) {
    U_ASSERT(key);
    if (table->m_fieldsStored == 0)
        return nullptr;
    const size_t fieldsNum = table->m_fieldsNum;
    if (fieldsNum <= 4) {
        // Just do a direct scan in the table
        for (size_t i = 0; i < fieldsNum; i++) {
            Field *field = &table->m_fields[i];
            if (field->m_nameLength == keyLength
                && (field->m_name == key ||
                ((keyLength == 0 || key[0] == field->m_name[0]) &&
                 (keyLength == 1 || key[1] == field->m_name[1]) &&
                memcmp(key, field->m_name, keyLength) == 0)))
            {
                return &field->m_value;
            }
        }
    } else {
        // Otherwise do the hash scan
        const size_t fieldsMask = fieldsNum - 1;
        for (size_t i = 0; i < fieldsNum; i++) {
            const size_t k = (hash + i) & fieldsMask;
            Field *field = &table->m_fields[k];
            if (!field->m_name)
                return nullptr;
            if (field->m_nameLength == keyLength
                && (field->m_name == key ||
                ((keyLength == 0 || key[0] == field->m_name[0]) &&
                 (keyLength == 1 || key[1] == field->m_name[1]) &&
                 memcmp(key, field->m_name, keyLength) == 0)))
            {
                return &field->m_value;
            }
        }
    }
    return nullptr;
}

void **Table::lookupReferenceWithHash(Table *table, const char *key, size_t keyLength, size_t hash) {
    return lookupReferenceWithHashInternal(table, key, keyLength, hash);
}

void **Table::lookupReference(Table *table, const char *key, size_t keyLength) {
    const size_t hash = table->m_fieldsNum > 4 ? djb2(key, keyLength) : 0;
    return lookupReferenceWithHashInternal(table, key, keyLength, hash);
}

// Versions which allocate
void **Table::lookupReferenceAllocWithHashInternal(Table *table,
                                                   const char *key,
                                                   size_t keyLength,
                                                   size_t keyHash,
                                                   void ***first)
{
    U_ASSERT(key);
    *first = nullptr;
    const size_t fieldsNum = table->m_fieldsNum;
    const size_t fieldsMask = fieldsNum - 1;
    size_t newLength;
    if (fieldsNum == 0) {
        newLength = 4;
    } else {
        Field *free = nullptr;
        for (size_t i = 0; i < fieldsNum; i++) {
            const size_t k = (keyHash + i) & fieldsMask;
            Field *field = &table->m_fields[k];
            if (field->m_nameLength == keyLength
                && (field->m_name == key ||
                ((keyLength == 0 || key[0] == field->m_name[0]) &&
                 (keyLength == 1 || key[1] == field->m_name[1]) &&
                 memcmp(key, field->m_name, keyLength) == 0)))
            {
                return &field->m_value;
            }
            if (!field->m_name) {
                free = field;
                break;
            }
        }
        const size_t fillRate = (table->m_fieldsStored * 100) / fieldsNum;
        if (fillRate < 70) {
            U_ASSERT(free);
            free->m_name = key;
            free->m_nameLength = keyLength;
            table->m_fieldsStored++;
            *first = &free->m_value;
            return nullptr;
        }
        newLength = fieldsNum * 2;
    }
    Table newTable;
    newTable.m_fields = (Field *)neoCalloc(sizeof(Field), newLength);
    newTable.m_fieldsNum = newLength;
    newTable.m_fieldsStored = 0;
    if (table->m_fieldsStored) {
        for (size_t i = 0; i < fieldsNum; i++) {
            Field *field = &table->m_fields[i];
            if (field->m_name) {
                void **freeObject;
                void **lookupObject = lookupReferenceAlloc(&newTable, field->m_name, field->m_nameLength, &freeObject);
                U_ASSERT(!lookupObject);
                *freeObject = field->m_value;
            }
        }
    }
    neoFree(table->m_fields);
    *table = newTable;
    return lookupReferenceAlloc(table, key, keyLength, first);
}

void **Table::lookupReferenceAllocWithHash(Table *table,
                                           const char *key,
                                           size_t keyLength,
                                           size_t keyHash,
                                           void ***first)
{
    return lookupReferenceAllocWithHashInternal(table, key, keyLength, keyHash, first);
}

void **Table::lookupReferenceAlloc(Table *table, const char *key, size_t keyLength, void ***first) {
    const size_t keyHash = table->m_fieldsNum ? djb2(key, keyLength) : 0;
    return lookupReferenceAllocWithHashInternal(table, key, keyLength, keyHash, first);
}

void *Table::lookup(Table *table, const char *key, size_t keyLength, bool *found) {
    const size_t keyHash = table->m_fieldsNum > 4 ? djb2(key, keyLength) : 0;
    void **search = lookupReferenceWithHash(table, key, keyLength, keyHash);
    if (!search) {
        if (found)
            *found = false;
        return nullptr;
    }
    if (found)
        *found = true;
    return *search;
}

void *Table::lookupWithHash(Table *table, const char *key, size_t keyLength, size_t keyHash, bool *found) {
    void **search = lookupReferenceWithHash(table, key, keyLength, keyHash);
    if (!search) {
        if (found)
            *found = false;
        return nullptr;
    }
    if (found)
        *found = true;
    return *search;
}

///! Object
Object *Object::lookupWithHash(Object *object, const char *key, size_t keyLength, size_t keyHash, bool *found) {
    while (object) {
        bool contains = false;
        Object *value = (Object *)Table::lookupWithHash(&object->m_table, key, keyLength, keyHash, &contains);
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

Object *Object::lookup(Object *object, const char *key, bool *found) {
    const size_t keyLength = strlen(key);
    const size_t keyHash = djb2(key, keyLength);
    return lookupWithHash(object, key, keyLength, keyHash, found);
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
        Table *table = &object->m_table;
        for (size_t i = 0; i < table->m_fieldsNum; i++) {
            Field *field = &table->m_fields[i];
            if (field->m_name)
                mark(state, (Object *)field->m_value);
        }

        // run any custom mark functions if they exist
        Object *current = object;
        while (current) {
            if (current->m_mark)
                current->m_mark(state, object);
            current = current->m_parent;
        }
    }
}

void Object::free(Object *object) {
    neoFree(object->m_table.m_fields);
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
    const size_t keyLength = strlen(key);
    const size_t keyHash = djb2(key, keyLength);
    while (current) {
        Object **search = (Object **)Table::lookupReferenceWithHash(&current->m_table, key, keyLength, keyHash);
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
    const size_t keyLength = strlen(key);
    const size_t keyHash = djb2(key, keyLength);
    while (current) {
        Object **search = (Object **)Table::lookupReferenceWithHash(&current->m_table, key, keyLength, keyHash);
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
    void **free = nullptr;
    Object **search = (Object **)Table::lookupReferenceAlloc(&object->m_table, key, strlen(key), &free);
    if (search) {
        U_ASSERT(!(object->m_flags & kImmutable));
    } else {
        U_ASSERT(!(object->m_flags & kClosed));
        search = (Object **)free;
    }
    *search = value;
}

void *Object::allocate(State *state, size_t size) {
    if (state->m_shared->m_gcState.m_numObjectsAllocated > state->m_shared->m_gcState.m_nextRun) {
        GC::run(state);
        // run gc after 20% growth or 10k elements
        state->m_shared->m_gcState.m_nextRun = (int)(state->m_shared->m_gcState.m_numObjectsAllocated * 1.5) + 10000;
    }

    Object *result = (Object *)neoCalloc(size, 1);
    result->m_prev = state->m_shared->m_gcState.m_lastObjectAllocated;
    result->m_size = size;
    state->m_shared->m_gcState.m_lastObjectAllocated = result;
    state->m_shared->m_gcState.m_numObjectsAllocated++;

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

Object *Object::newBoolUncached(State *state, bool value) {
    Object *boolBase = lookup(state->m_root, "bool", nullptr);
    BoolObject *object = (BoolObject *)allocate(state, sizeof *object);
    object->m_parent = boolBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newBool(State *state, bool value) {
    if (value) {
        return state->m_shared->m_valueCache.m_boolTrue;
    }
    return state->m_shared->m_valueCache.m_boolFalse;
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

Object *Object::newArray(State *state, Object **contents, IntObject *length) {
    Object *arrayBase = lookup(state->m_root, "array", nullptr);
    ArrayObject *object = (ArrayObject *)allocate(state, sizeof *object);
    object->m_parent = arrayBase;
    object->m_contents = contents;
    object->m_length = length->m_value;
    setNormal((Object *)object, "length", (Object *)length);
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
