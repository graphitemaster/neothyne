#ifndef S_MEMORY_HDR
#define S_MEMORY_HDR

#include "u_set.h"

namespace s {

// This implements an object tracker used during lexing, parsing and code generation
// to make it easier to deal with memory management. On lexing, parsing or code
// generation should an error arise no memory cleanup must be handled by the
// compiler, instead anything partial tracked by this gets cleaned up automatically.
//
// The exception to the rule is when objects are allocated but destroyed in the
// normal case where we don't need them anymore. Those situations still require
// a call to free. The call can be omitted and this memory tracker will indeed
// free it later but it would still be a leak because it would stay alive too
// long and many of them would collect over time.

struct Memory {
    static void init(Memory *memory);
    static void destroy(Memory *memory);

    U_MALLOC_LIKE static void *allocate(Memory *memory, size_t size);
    U_MALLOC_LIKE static void *allocate(Memory *memory, size_t count, size_t size);
    U_MALLOC_LIKE static void *reallocate(Memory *memory, void *old, size_t resize);

    static void free(Memory *memory, void *old);

private:
    static bool addMember(Memory *memory, void *member);

    static bool add(Memory *memory, void *member);
    static bool del(Memory *memory, void *member);

    static void maybeRehash(Memory *memory);

    size_t m_numBits;
    size_t m_mask;
    size_t m_capacity;
    void **m_items;
    size_t m_numItems;
    size_t m_numDeletedItems;
};

}

#endif
