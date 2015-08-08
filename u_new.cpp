#include <stdlib.h>
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

voidptr neoMalloc(size_t size) {
    const size_t offset = kMemoryAlignment - 1 + sizeof(void*);
    voidptr data = neoCoreMalloc(size + offset);
    void **store = (void **)(((size_t)((void *)data) + offset) & ~(kMemoryAlignment - 1));
    store[-1] = data;
    return store;
}

voidptr neoRealloc(voidptr what, size_t size) {
    const size_t offset = kMemoryAlignment - 1 + sizeof(void*);
    voidptr data = neoCoreRealloc(what ? ((void **)what)[-1] : nullptr, size + offset);
    void **store = (void **)(((size_t)((void *)data) + offset) & ~(kMemoryAlignment - 1));
    store[-1] = data;
    return store;
}

void neoFree(voidptr what) {
    if (what)
        neoCoreFree(((void **)what)[-1]);
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

