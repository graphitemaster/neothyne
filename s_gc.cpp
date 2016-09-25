#include "s_gc.h"
#include "s_object.h"

namespace s {

static GC::State state;

void *GC::addRoots(Object **objects, size_t length) {
    RootSet *prevTail = state.m_tail;
    state.m_tail = (RootSet *)neoMalloc(sizeof(RootSet));
    if (prevTail)
        prevTail->m_next = state.m_tail;
    state.m_tail->m_prev = prevTail;
    state.m_tail->m_next = nullptr;
    state.m_tail->m_objects = objects;
    state.m_tail->m_length = length;
    return (void *)state.m_tail;
}

void GC::removeRoots(void *base) {
    RootSet *entry = (RootSet *)base;
    if (entry == state.m_tail)
        state.m_tail = entry->m_prev;
    if (entry->m_prev)
        entry->m_prev->m_next = entry->m_next;
    if (entry->m_next)
        entry->m_next->m_prev = entry->m_prev;
    neoFree(entry);
}

// mark roots
void GC::mark() {
    RootSet *set = state.m_tail;
    while (set) {
        for (size_t i = 0; i < set->m_length; i++) {
            if (set->m_objects[i])
                set->m_objects[i]->mark();
        }
        set = set->m_prev;
    }
}

// scan allocated objects freeing those that are not marked
void GC::sweep() {
    Object **current = &Object::m_lastAllocated;
    while (*current) {
        if (!((*current)->m_flags & Object::kMarked)) {
            Object *prev = (*current)->m_prev;
            (*current)->free();
            Object::m_numAllocated--;
            *current = prev;
        } else {
            (*current)->m_flags &= ~Object::kMarked;
            current = &(*current)->m_prev;
        }
    }
}

void GC::run() {
    mark();
    sweep();
}

}
