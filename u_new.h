#ifndef U_NEW_HDR
#define U_NEW_HDR
#include <stddef.h>

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
