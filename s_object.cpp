#include <string.h>

#include "s_object.h"
#include "s_runtime.h"

#include "u_new.h"
#include "u_assert.h"
#include "u_log.h"

namespace s {

///! UserFunction
void UserFunction::dump() {
    FunctionBody *body = &m_body;
    u::Log::out("[script] => fn %s (%zu), %zu slots [\n", m_name, m_arity, m_slots);
    for (size_t i = 0; i < body->m_length; i++) {
        u::Log::out("[script] =>   block <%zu> [\n", i);
        InstrBlock *block = &body->m_blocks[i];
        for (size_t j = 0; j < block->m_length; j++) {
            Instr *instruction = block->m_instrs[j];
            instruction->dump();
        }
        u::Log::out("[script] =>   ]\n");
    }
    u::Log::out("[script] => ]\n");
}

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
void Object::set(const char *key, Object *value) {
    U_ASSERT(!(m_flags & kClosed));
    Table::Entry *free = nullptr;
    Object **find = m_table.lookup(key, &free);
    if (!find) {
        free->m_name = key;
        find = &free->m_value;
    }
    *find = value;
}

Object *Object::newObject(Object *parent) {
    Object *object = allocate<Object>();
    object->m_parent = parent;
    return object;
}

Object *Object::newInt(Object *context, int value) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *intBase = root->m_table.lookup("int");
    auto *object = allocate<IntObject>();
    object->m_parent = intBase;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newBoolean(Object *context, bool value) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *booleanBase = root->m_table.lookup("boolean");
    auto *object = allocate<BooleanObject>();
    object->m_parent = booleanBase;
    object->m_value = value;
    return (Object *)object;
}

Object *Object::newFunction(Object *context, FunctionPointer function) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *functionBase = root->m_table.lookup("function");
    auto *object = allocate<FunctionObject>();
    object->m_parent = functionBase;
    object->m_function = function;
    return (Object *)object;
}

Object *Object::newUserFunction(Object *context, UserFunction *function) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *functionBase = root->m_table.lookup("function");
    auto *object = allocate<UserFunctionObject>();
    object->m_parent = functionBase;
    object->m_function = handler;
    object->m_userFunction = *function;
    return (Object *)object;
}

}
