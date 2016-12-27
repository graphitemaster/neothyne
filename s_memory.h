#ifndef S_MEMORY_HDR
#define S_MEMORY_HDR

#include <stddef.h>
#include <stdint.h>

#include "u_traits.h"

namespace s {

struct Memory {
    static void init();
    static void destroy();

    U_MALLOC_LIKE static void *allocate(size_t size);
    U_MALLOC_LIKE static void *allocate(size_t count, size_t size);
    U_MALLOC_LIKE static void *reallocate(void *old, size_t resize);

    static void free(void *old);

private:
    struct alignas(16) Header {
        size_t size;
    };

    static bool addMember(uintptr_t member);

    static bool add(uintptr_t member);
    static bool del(uintptr_t member);

    static void maybeRehash();

    [[noreturn]]
    static void oom(size_t requested);

    static size_t m_numBits;
    static size_t m_mask;
    static size_t m_capacity;
    static uintptr_t *m_items;
    static size_t m_numItems;
    static size_t m_numDeletedItems;
    static size_t m_bytesAllocated;
};

}

#endif
