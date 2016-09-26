#include "s_gc.h"
#include "s_object.h"

namespace s {

static GarbageCollector::State gState;

void *GarbageCollector::addRoots(Object **objects, size_t length) {
    RootSet *tail = gState.m_tail;
    gState.m_tail = (RootSet *)neoMalloc(sizeof *gState.m_tail);
    if (tail)
        tail->m_next = gState.m_tail;
    gState.m_tail->m_prev = tail;
    gState.m_tail->m_next = nullptr;
    gState.m_tail->m_objects = objects;
    gState.m_tail->m_length = length;
    return gState.m_tail;
}

void GarbageCollector::delRoots(void *root) {
    RootSet *object = (RootSet *)root;
    if (object == gState.m_tail)
        gState.m_tail = object->m_prev;
    if (object->m_prev)
        object->m_prev->m_next = object->m_next;
    if (object->m_next)
        object->m_next->m_prev = object->m_prev;
    neoFree(object);
}

void GarbageCollector::mark(Object *context) {
    RootSet *tail = gState.m_tail;
    while (tail) {
        for (size_t i = 0; i < tail->m_length; i++)
            Object::mark(context, tail->m_objects[i]);
        tail = tail->m_prev;
    }
}

void GarbageCollector::sweep() {
    // TODO(daleweiler): cleanup
    Object **current = &Object::m_lastAllocated;
    while (*current) {
        if (!((*current)->m_flags & Object::kMarked)) {
            Object *prev = (*current)->m_prev;
            Object::free(*current);
            Object::m_numAllocated--;
            *current = prev;
        } else {
            (*current)->m_flags &= ~Object::kMarked;
            current = &(*current)->m_prev;
        }
    }
}

void GarbageCollector::run(Object *context) {
    mark(context);
    sweep();
}

}
