#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "u_new.h"
#include "u_misc.h"
#include "u_assert.h"
#include "u_traits.h"

static constexpr size_t kMemoryAlignment = 16u;

static inline U_MALLOC_LIKE void *neoCoreMalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr)
        return ptr;
    U_ASSERT(0 && "Out of memory");
    abort();
}

static inline U_MALLOC_LIKE void *neoCoreRealloc(void *ptr, size_t size) {
    if (size) {
        void *resize = realloc(ptr, size);
        if (resize)
            return resize;
        U_ASSERT(0 && "Out of memory");
        abort();
    }
    U_UNREACHABLE();
}

static inline void neoCoreFree(void *what) {
    free(what);
}

// when alignof(max_align_t) is < kMemoryAlignment we provide our own wrapper
// implementation that guarantees alignment. Otherwise we use the system
// provided allocator
template <bool>
struct alignedAllocator {
    static U_MALLOC_LIKE void *neoMalloc(size_t size) {
        return neoCoreMalloc(size);
    }

    static U_MALLOC_LIKE void *neoRealloc(void *what, size_t size) {
        return neoCoreRealloc(what, size);
    }

    static void neoFree(void *what) {
        neoCoreFree(what);
    }
};

// Aligned malloc on its own is trivial, aligned realloc however isn't. A clever
// technique is used here to avoid overhead. Instead of storing the base pointer
// and the size of the request allocation (as required for realloc) we instead
// store the size rounded to a multiple of kMemoryAlignment, which gives us some
// bits to also store the difference between the aligned pointer and base pointer.
template <>
inline U_MALLOC_LIKE void *alignedAllocator<true>::neoMalloc(size_t size) {
    size = (size+(kMemoryAlignment-1)) & ~(kMemoryAlignment-1);
    unsigned char *base = (unsigned char *)neoCoreMalloc(size + sizeof(size_t) + kMemoryAlignment);
    unsigned char *aligned = (unsigned char *)((uintptr_t)(base + sizeof(size_t) + (kMemoryAlignment - 1)) & ~(kMemoryAlignment - 1));
    ((size_t *)aligned)[-1] = size|(aligned-base-sizeof(size_t));
    return aligned;
}

template <>
inline void alignedAllocator<true>::neoFree(void *what) {
    if (what)
        neoCoreFree((unsigned char *)what - sizeof(size_t) - (((size_t*)what)[-1] & (kMemoryAlignment-1)));
}

template <>
inline U_MALLOC_LIKE void *alignedAllocator<true>::neoRealloc(void *what, size_t size) {
    if (what) {
        size = (size+(kMemoryAlignment-1)) & ~(kMemoryAlignment-1);
        unsigned char *original = (unsigned char *)what - sizeof(size_t) - (((size_t*)what)[-1] & (kMemoryAlignment-1));
        const size_t originalSize = ((size_t *)what)[-1] & ~(kMemoryAlignment-1);
        const ptrdiff_t shift = (unsigned char *)what - original;
        unsigned char *resize = (unsigned char *)neoCoreRealloc(original, size + sizeof(size_t) + kMemoryAlignment);
        unsigned char *aligned = (unsigned char *)((uintptr_t)(resize + sizeof(size_t) + (kMemoryAlignment - 1)) & ~(kMemoryAlignment - 1));
        // relative shift of data can be different than from before, this will only move what is absolutely necessary
        if (shift != aligned-resize)
            u::moveMemory(aligned, resize+shift, originalSize);
        ((size_t *)aligned)[-1] = size|(aligned-resize-sizeof(size_t));
        return aligned;
    }
    return neoMalloc(size);
}

struct max_alignment {
    struct data {
        long long x;
        long double y;
    };
    enum { value = alignof(data) };
};

// sizeof(long double) in Visual Studio is not 16 bytes for Win64 even though it should be.
// this works around the non-conformancy of Visual Studio.
#if defined(_WIN64)
using allocator = alignedAllocator<false>;
#else
using allocator = alignedAllocator<max_alignment::value < kMemoryAlignment>;
#endif

U_MALLOC_LIKE void *neoMalloc(size_t size) {
    return allocator::neoMalloc(size);
}

U_MALLOC_LIKE void *neoRealloc(void *ptr, size_t size) {
    return allocator::neoRealloc(ptr, size);
}

U_MALLOC_LIKE void *neoCalloc(size_t size, size_t count) {
    U_ASSERT(!(count && size > -1_z/count)); // overflow
    const size_t sum = size * count;
    // round memory requirement to multiple of sizeof(size_t) such that
    // zero page optimization below does not cross memory boundary when
    // writing zeros
    const size_t bytes = (sum + sizeof(size_t)) & ~(sizeof(size_t) - 1);
    U_ASSERT(bytes > sum); // overflow
    void *p = allocator::neoMalloc(bytes);
    // zero page optimization
    size_t n = (bytes + sizeof n - 1) / sizeof n;
    for (size_t *z = (size_t *)p; n; n--, z++) if (*z) *z = 0;
    return p;
}

void neoFree(void *ptr) {
    allocator::neoFree(ptr);
}

U_MALLOC_LIKE void *operator new(size_t size) noexcept {
    return neoMalloc(size);
}

U_MALLOC_LIKE void *operator new[](size_t size) noexcept {
    return neoMalloc(size);
}

void operator delete(void *ptr) noexcept {
    neoFree(ptr);
}

void operator delete[](void *ptr) noexcept {
    neoFree(ptr);
}

// These are part of the C++ ABI on all systems except MSVC.
#if !defined(_MSC_VER)
extern "C" void __cxa_pure_virtual() {
    U_ASSERT(0 && "pure virtual function call");
}

extern "C" void __cxa_guard_acquire(...) {
    // We don't require thread-safe initialization of globals
}
extern "C" void __cxa_guard_release(...) {
    // We don't require thread-safe initialization of globals
}
#endif
