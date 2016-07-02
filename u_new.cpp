#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "u_new.h"
#include "u_misc.h"
#include "u_assert.h"
#include "u_traits.h"

static constexpr size_t kMemoryAlignment = 16u;

static inline voidptr neoCoreMalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr)
        return ptr;
    abort();
}

static inline voidptr neoCoreRealloc(voidptr ptr, size_t size) {
    if (size) {
        void *resize = realloc(ptr, size);
        if (resize)
            return resize;
    }
    abort();
}

static inline void neoCoreFree(voidptr what) {
    free(what);
}

// when alignof(max_align_t) is < kMemoryAlignment we provide our own wrapper
// implementation that guarantees alignment. Otherwise we use the system
// provided allocator
template <bool>
struct alignedAllocator {
    static voidptr neoMalloc(size_t size) {
        return neoCoreMalloc(size);
    }

    static voidptr neoRealloc(voidptr what, size_t size) {
        return neoCoreRealloc(what, size);
    }

    static void neoFree(voidptr what) {
        neoCoreFree(what);
    }
};

// Aligned malloc on its own is trivial, aligned realloc however isn't. A clever
// technique is used here to avoid overhead. Instead of storing the base pointer
// and the size of the request allocation (as required for realloc) we instead
// store the size rounded to a multiple of kMemoryAlignment, which gives us some
// bits to also store the difference between the aligned pointer and base pointer.
template <>
inline voidptr alignedAllocator<true>::neoMalloc(size_t size) {
    size = (size+(kMemoryAlignment-1)) & ~(kMemoryAlignment-1);
    unsigned char *base = neoCoreMalloc(size + sizeof(size_t) + kMemoryAlignment);
    unsigned char *aligned = (unsigned char *)((uintptr_t)(base + sizeof(size_t) + (kMemoryAlignment - 1)) & ~(kMemoryAlignment - 1));
    ((size_t *)aligned)[-1] = size|(aligned-base-sizeof(size_t));
    return aligned;
}

template <>
inline void alignedAllocator<true>::neoFree(voidptr what) {
    if (what)
        neoCoreFree((unsigned char *)what - sizeof(size_t) - (((size_t*)what)[-1] & (kMemoryAlignment-1)));
}

template <>
inline voidptr alignedAllocator<true>::neoRealloc(voidptr what, size_t size) {
    if (what) {
        if (size == 0) {
            neoFree(what);
            return nullptr;
        }
        size = (size+(kMemoryAlignment-1)) & ~(kMemoryAlignment-1);
        unsigned char *original = (unsigned char *)what - sizeof(size_t) - (((size_t*)what)[-1] & (kMemoryAlignment-1));
        const size_t originalSize = ((size_t *)what)[-1] & ~(kMemoryAlignment-1);
        const ptrdiff_t shift = (unsigned char *)what - original;
        unsigned char *resize = neoCoreRealloc(original, size + sizeof(size_t) + kMemoryAlignment);
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

voidptr neoMalloc(size_t size) {
    return allocator::neoMalloc(size);
}

voidptr neoRealloc(voidptr ptr, size_t size) {
    return allocator::neoRealloc(ptr, size);
}

void neoFree(voidptr ptr) {
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
