#include <stdlib.h>
#include <assert.h>

#include "u_new.h"

voidptr neoMalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) abort();
    return ptr;
}

voidptr neoRealloc(voidptr ptr, size_t size) {
    if (!size) abort();
    void *resize = realloc(ptr, size);
    if (!resize) abort();
    return resize;
}

voidptr neoAlignedMalloc(size_t size, size_t alignment) {
    size_t offset = alignment - 1 + sizeof(void*);
    voidptr data = neoMalloc(size + offset);
    void **store = (void**)(((size_t)((void *)data) + offset) & ~(alignment - 1));
    store[-1] = data;
    return store;
}

extern "C" void __cxa_pure_virtual() {
    assert(0 && "pure virtual function call");
    abort();
}

void neoAlignedFree(voidptr ptr) {
    neoFree(((void**)ptr)[-1]);
}

void neoFree(voidptr what) {
    free(what);
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
