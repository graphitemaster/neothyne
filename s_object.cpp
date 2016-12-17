#include <string.h>

#include "s_object.h"
#include "s_runtime.h"
#include "s_memory.h"
#include "s_gc.h"

#include "u_new.h"
#include "u_assert.h"
#include "u_log.h"

namespace s {

///! Table
Field *Table::lookupWithHashInternalUnroll(Table *table, const char *key, size_t keyLength, size_t hash) {
    U_ASSERT(key);
    if (table->m_fieldsStored == 0)
        return nullptr;
    if ((table->m_bloom & hash) != hash)
        return nullptr;
    const size_t fieldsNum = table->m_fieldsNum;
    if (fieldsNum <= 8) {
        // Just do a direct scan in the table
        for (size_t i = 0; i < fieldsNum; i++) {
            Field *field = &table->m_fields[i];
            if (field->m_nameLength == keyLength
                && (field->m_name == key ||
                ((keyLength == 0 || key[0] == field->m_name[0]) &&
                 (keyLength <= 1 || key[1] == field->m_name[1]) &&
                memcmp(key, field->m_name, keyLength) == 0)))
            {
                return field;
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
                 (keyLength <= 1 || key[1] == field->m_name[1]) &&
                 memcmp(key, field->m_name, keyLength) == 0)))
            {
                return field;
            }
        }
    }
    return nullptr;
}

Field *Table::lookupWithHashInternal(Table *table, const char *key, size_t keyLength, size_t hash) {
    // Some partial unrolling here for short keys
    if (U_UNLIKELY(keyLength == 0))
        return lookupWithHashInternalUnroll(table, key, 0, hash);
    if (U_UNLIKELY(keyLength == 1))
        return lookupWithHashInternalUnroll(table, key, 1, hash);
    if (U_UNLIKELY(keyLength == 2))
        return lookupWithHashInternalUnroll(table, key, 2, hash);
    return lookupWithHashInternalUnroll(table, key, keyLength, hash);
}

Field *Table::lookupWithHash(Table *table, const char *key, size_t keyLength, size_t hash) {
    return lookupWithHashInternal(table, key, keyLength, hash);
}

Field *Table::lookup(Table *table, const char *key, size_t keyLength) {
    const size_t hash = table->m_fieldsNum > 8 ? djb2(key, keyLength) : 0;
    return lookupWithHashInternal(table, key, keyLength, hash);
}

// Versions which allocate
Field *Table::lookupAllocWithHashInternal(Table *table,
                                          const char *key,
                                          size_t keyLength,
                                          size_t keyHash,
                                          Field **first)
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
                 (keyLength <= 1 || key[1] == field->m_name[1]) &&
                 memcmp(key, field->m_name, keyLength) == 0)))
            {
                return field;
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
            table->m_bloom |= keyHash;
            *first = free;
            return nullptr;
        }
        newLength = fieldsNum * 2;
    }
    Table newTable;
    newTable.m_fields = (Field *)Memory::allocate(sizeof(Field), newLength);
    newTable.m_fieldsNum = newLength;
    newTable.m_fieldsStored = 0;
    newTable.m_bloom = 0;
    if (table->m_fieldsStored) {
        for (size_t i = 0; i < fieldsNum; i++) {
            Field *field = &table->m_fields[i];
            if (field->m_name) {
                Field *freeObject = nullptr;
                Field *lookupObject = lookupAlloc(&newTable,
                                                  field->m_name,
                                                  field->m_nameLength,
                                                  &freeObject);
                U_ASSERT(!lookupObject);
                freeObject->m_value = field->m_value;
            }
        }
    }
    Memory::free(table->m_fields);
    *table = newTable;
    return lookupAlloc(table, key, keyLength, first);
}

Field *Table::lookupAllocWithHash(Table *table,
                                  const char *key,
                                  size_t keyLength,
                                  size_t keyHash,
                                  Field **first)
{
    return lookupAllocWithHashInternal(table, key, keyLength, keyHash, first);
}

Field *Table::lookupAlloc(Table *table, const char *key, size_t keyLength, Field **first) {
    const size_t keyHash = table->m_fieldsNum ? djb2(key, keyLength) : 0;
    return lookupAllocWithHashInternal(table, key, keyLength, keyHash, first);
}

///! Object
Object **Object::lookupReferenceWithHash(Object *object, const char *key, size_t keyLength, size_t keyHash) {
    while (object) {
        Object **value = (Object **)&Table::lookupWithHash(&object->m_table, key, keyLength, keyHash)->m_value;
        if (value)
            return value;
        object = object->m_parent;
    }
    return nullptr;
}

Object **Object::lookupReference(Object *object, const char *key) {
    const size_t keyLength = strlen(key);
    const size_t keyHash = djb2(key, keyLength);
    return lookupReferenceWithHash(object, key, keyLength, keyHash);
}

Object *Object::lookupWithHash(Object *object, const char *key, size_t keyLength, size_t keyHash, bool *keyFound) {
    // Hoised the invariant out of the loop to avoid a branch every iteration;
    // GCC failed to do this for us.
    if (keyFound) {
        while (object) {
            Field *field = Table::lookupWithHash(&object->m_table, key, keyLength, keyHash);
            if (field) {
                *keyFound = true;
                return (Object *)field->m_value;
            }
            object = object->m_parent;
        }
        *keyFound = false;
    } else {
        while (object) {
            Field *field = Table::lookupWithHash(&object->m_table, key, keyLength, keyHash);
            if (field)
                return (Object *)field->m_value;
            object = object->m_parent;
        }
    }
    return nullptr;
}

Object *Object::lookup(Object *object, const char *key, bool *keyFound) {
    const size_t keyLength = strlen(key);
    const size_t keyHash = djb2(key, keyLength);
    return lookupWithHash(object, key, keyLength, keyHash, keyFound);
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
    Memory::free(object->m_table.m_fields);
    Memory::free(object);
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
        Field *field = Table::lookupWithHash(&current->m_table, key, keyLength, keyHash);
        if (field) {
            if (current->m_flags & kImmutable)
                return false;
            field->m_value = (void *)value;
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
        Field *field = Table::lookupWithHash(&current->m_table, key, keyLength, keyHash);
        if (field) {
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
    Field *field = Table::lookupAlloc(&object->m_table, key, strlen(key), &free);
    if (field) {
        U_ASSERT(!(object->m_flags & kImmutable));
        field->m_value = (void *)value;
    } else {
        U_ASSERT(!(object->m_flags & kClosed));
        free->m_value = (void *)value;
    }
}

void *Object::allocate(State *state, size_t size) {
    if (state->m_shared->m_gcState.m_numObjectsAllocated > state->m_shared->m_gcState.m_nextRun) {
        GC::run(state);
        // run gc after 20% growth or 10k elements
        state->m_shared->m_gcState.m_nextRun = (int)(state->m_shared->m_gcState.m_numObjectsAllocated * 1.5) + 10000;
    }

    Object *result = (Object *)Memory::allocate(size, 1);
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
    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    IntObject *object = (IntObject *)allocate(state, sizeof *object);
    object->m_parent = intBase;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newFloat(State *state, float value) {
    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;
    FloatObject *object = (FloatObject *)allocate(state, sizeof *object);
    object->m_parent = floatBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newBoolUncached(State *state, bool value) {
    Object *boolBase = state->m_shared->m_valueCache.m_boolBase;
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

Object *Object::newString(State *state, const char *value, size_t length) {
    // string gets allocated as part of the object
    StringObject *object = (StringObject *)allocate(state, sizeof *object + length + 1);
    object->m_parent = state->m_shared->m_valueCache.m_stringBase;
    object->m_flags = kImmutable | kClosed;
    object->m_value = ((char *)object) + sizeof *object;
    strncpy(object->m_value, value, length + 1);
    return (Object *)object;
}

Object *Object::newArray(State *state, Object **contents, IntObject *length) {
    ArrayObject *object = (ArrayObject *)allocate(state, sizeof *object);
    object->m_parent = state->m_shared->m_valueCache.m_arrayBase;
    object->m_contents = contents;
    object->m_length = length->m_value;
    setNormal((Object *)object, "length", (Object *)length);
    return (Object *)object;
}

Object *Object::newFunction(State *state, FunctionPointer function) {
    Object *functionBase = state->m_shared->m_valueCache.m_functionBase;
    FunctionObject *object = (FunctionObject*)allocate(state, sizeof *object);
    object->m_parent = functionBase;
    object->m_function = function;
    return (Object *)object;
}

}
