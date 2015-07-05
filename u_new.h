#ifndef U_NEW_HDR
#define U_NEW_HDR
#include <stddef.h>

// The following class type reintroduces C's implicit conversion of pointer
// types to and from void*.
struct voidptr {
    voidptr(void *ptr);
    template <typename T>
    operator T *();
private:
    void *m_ptr;
};

inline voidptr::voidptr(void *ptr)
    : m_ptr(ptr)
{
}

template <typename T>
inline voidptr::operator T*() {
    return (T*)m_ptr;
}

// Raw memory allocation mechanism.
voidptr neoMalloc(size_t size);
voidptr neoRealloc(voidptr ptr, size_t size);
void neoFree(voidptr ptr);
voidptr neoAlignedMalloc(size_t size, size_t alignment);
void neoAlignedFree(voidptr ptr);

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
