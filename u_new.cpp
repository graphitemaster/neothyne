#include <stdlib.h>

void *operator new(size_t len) noexcept {
    void *ptr = malloc(len);
    if (!ptr)
        abort();
    return ptr;
}

void *operator new[](size_t len) noexcept {
    void *ptr = malloc(len);
    if (!ptr)
        abort();
    return ptr;
}

void operator delete(void *ptr) noexcept {
    free(ptr);
}

void operator delete[](void *ptr) noexcept {
    free(ptr);
}
