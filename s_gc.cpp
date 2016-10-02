#include "s_gc.h"
#include "s_object.h"

namespace s {

void GC::addRoots(State *state, Object **objects, size_t count, RootSet *set) {
    RootSet *prevTail = state->m_gc.m_tail;
    state->m_gc.m_tail = set;
    if (prevTail)
        prevTail->m_next = state->m_gc.m_tail;
    state->m_gc.m_tail->m_prev = prevTail;
    state->m_gc.m_tail->m_next = nullptr;
    state->m_gc.m_tail->m_objects = objects;
    state->m_gc.m_tail->m_count = count;
}

void GC::delRoots(State *state, RootSet *entry) {
    if (entry == state->m_gc.m_tail)
        state->m_gc.m_tail = entry->m_prev;
    if (entry->m_prev)
        entry->m_prev->m_next = entry->m_next;
    if (entry->m_next)
        entry->m_next->m_prev = entry->m_prev;
}

void GC::mark(State *state) {
    RootSet *set = state->m_gc.m_tail;
    while (set) {
        for (size_t i = 0; i < set->m_count; i++)
            Object::mark(state, set->m_objects[i]);
        set = set->m_prev;
    }
}

void GC::sweep(State *state) {
    Object **current = &state->m_last;
    while (*current) {
        if ((*current)->m_flags & kMarked) {
            (*current)->m_flags &= ~kMarked;
            current = &(*current)->m_prev;
        } else {
            Object *prev = (*current)->m_prev;
            Object::free(*current);
            state->m_count--;
            *current = prev;
        }
    }
}

void GC::run(State *state) {
    mark(state);
    sweep(state);
}

}
