#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "u_new.h"

static constexpr size_t kMemoryAlignment = 16u;

static voidptr neoCoreMalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr)
        return ptr;
    abort();
}

static voidptr neoCoreRealloc(voidptr ptr, size_t size) {
    if (size) {
        void *resize = realloc(ptr, size);
        if (resize)
            return resize;
    }
    abort();
}

static void neoCoreFree(voidptr what) {
    free(what);
}

// when alignof(max_align_t) is < kMemoryAlignment we provide our own wrapper
// implementation that guarantees alignment. Otherwise we use the system
// provided allocator
template <bool>
struct alignedAllocator {
    using base = alignedAllocator;

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
voidptr alignedAllocator<true>::neoMalloc(size_t size) {
    size = (size+(kMemoryAlignment-1)) & ~(kMemoryAlignment-1);
    unsigned char *base = neoCoreMalloc(size + sizeof(size_t) + kMemoryAlignment);
    unsigned char *aligned = (unsigned char *)((uintptr_t)(base + sizeof(size_t) + (kMemoryAlignment - 1)) & ~(kMemoryAlignment - 1));
    ((size_t *)aligned)[-1] = size|(aligned-base-sizeof(size_t));
    return aligned;
}

template <>
void alignedAllocator<true>::neoFree(voidptr what) {
    if (what)
        neoCoreFree((unsigned char *)what - sizeof(size_t) - (((size_t*)what)[-1] & (kMemoryAlignment-1)));
}

template <>
voidptr alignedAllocator<true>::neoRealloc(voidptr what, size_t size) {
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
            memmove(aligned, resize+shift, originalSize);
        ((size_t *)aligned)[-1] = size|(aligned-resize-sizeof(size_t));
        return aligned;
    }
    return neoMalloc(size);
}

using allocator = alignedAllocator<alignof(max_align_t) < kMemoryAlignment>;

voidptr neoMalloc(size_t size) {
    return allocator::neoMalloc(size);
}

voidptr neoRealloc(voidptr ptr, size_t size) {
    return allocator::neoRealloc(ptr, size);
}

void neoFree(voidptr ptr) {
    allocator::neoFree(ptr);
}

void *operator new(size_t size) noexcept {
    return neoMalloc(size);
}

void *operator new[](size_t size) noexcept {
    return neoMalloc(size);
}

void operator delete(void *ptr) noexcept {
    neoFree(ptr);
}

void operator delete[](void *ptr) noexcept {
    neoFree(ptr);
}

extern "C" void __cxa_pure_virtual() {
    assert(0 && "pure virtual function call");
    abort();
}

