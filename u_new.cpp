#include <stdlib.h>
#include "u_new.h"

voidptr neoMalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) abort();
    return ptr;
}

voidptr neoRealloc(voidptr ptr, size_t size) {
    void *resize = realloc(ptr, size);
    if (!resize) abort();
    return resize;
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
