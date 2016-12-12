#include "s_gc.h"
#include "s_object.h"
#include "s_memory.h"

#include "u_assert.h"

namespace s {

void GC::init(State *state) {
    addRoots(state, nullptr, 0, &state->m_shared->m_gcState.m_permanents);
}

void GC::addPermanent(State *state, Object *object) {
    RootSet *permanents = &state->m_shared->m_gcState.m_permanents;
    permanents->m_objects = (Object **)Memory::reallocate(permanents->m_objects,
                                                          sizeof(Object *) * ++permanents->m_count);
    permanents->m_objects[permanents->m_count -1] = object;
}

void GC::addRoots(State *state, Object **objects, size_t count, RootSet *set) {
    RootSet *prevTail = state->m_shared->m_gcState.m_tail;
    state->m_shared->m_gcState.m_tail = set;
    if (prevTail) {
        prevTail->m_next = state->m_shared->m_gcState.m_tail;
    }
    state->m_shared->m_gcState.m_tail->m_prev = prevTail;
    state->m_shared->m_gcState.m_tail->m_next = nullptr;
    state->m_shared->m_gcState.m_tail->m_objects = objects;
    state->m_shared->m_gcState.m_tail->m_count = count;
}

void GC::delRoots(State *state, RootSet *entry) {
    if (entry == state->m_shared->m_gcState.m_tail) {
        state->m_shared->m_gcState.m_tail = entry->m_prev;
    }
    if (entry->m_prev) {
        entry->m_prev->m_next = entry->m_next;
    }
    if (entry->m_next) {
        entry->m_next->m_prev = entry->m_prev;
    }
}

void GC::mark(State *state) {
    RootSet *set = state->m_shared->m_gcState.m_tail;
    while (set) {
        for (size_t i = 0; i < set->m_count; i++) {
            Object::mark(state, set->m_objects[i]);
        }
        set = set->m_prev;
    }
}

void GC::sweep(State *state) {
    Object **current = &state->m_shared->m_gcState.m_lastObjectAllocated;
    while (*current) {
        if ((*current)->m_flags & kMarked) {
            (*current)->m_flags &= ~kMarked;
            current = &(*current)->m_prev;
        } else {
            Object *prev = (*current)->m_prev;
            Object::free(*current);
            state->m_shared->m_gcState.m_numObjectsAllocated--;
            *current = prev;
        }
    }
}

void GC::disable(State *state) {
    state->m_shared->m_gcState.m_disabledness++;
}

void GC::enable(State *state) {
    U_ASSERT(state->m_shared->m_gcState.m_disabledness);
    state->m_shared->m_gcState.m_disabledness--;
    if (state->m_shared->m_gcState.m_disabledness == 0 && state->m_shared->m_gcState.m_missed) {
        state->m_shared->m_gcState.m_missed = false;
        run(state);
    }
}

void GC::run(State *state) {
    if (state->m_shared->m_gcState.m_disabledness > 0) {
        state->m_shared->m_gcState.m_missed = true;
        return;
    }
    mark(state);
    sweep(state);
}

}
