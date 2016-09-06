#ifndef U_NEW_HDR
#define U_NEW_HDR
#include "u_traits.h"

// Raw memory allocation mechanism.
U_MALLOC_LIKE void *neoMalloc(size_t size);
U_MALLOC_LIKE void *neoRealloc(void *ptr, size_t size);
void neoFree(void *ptr);

inline void *operator new(size_t, void *ptr) {
    return ptr;
}

inline void *operator new[](size_t, void *ptr) {
    return ptr;
}

inline void operator delete(void *, void *) {
    // Do nothing
}

inline void operator delete[](void *, void *) {
    // Do nothing
}

#endif
