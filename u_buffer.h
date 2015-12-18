#ifndef U_BUFFER_HDR
#define U_BUFFER_HDR
#include <assert.h>
#include <stdlib.h>

#include "u_new.h"
#include "u_algorithm.h"

namespace u {

namespace detail {
    template <typename T, bool = u::is_pod<T>::value>
    struct is_pod {};
}

template <typename T>
struct buffer {
    buffer();
    buffer(buffer &&other);
    ~buffer();

    T *first;
    T *last;
    T *capacity;

    buffer &operator=(buffer &&other);

    void destroy();

    void resize(size_t size);
    void resize(size_t size, const T &value);
    void reserve(size_t icapacity);
    void clear();
    template <typename I>
    void insert(T *where, const I *ifirst, const I *ilast);
    void insert(T *where, size_t count);
    void swap(buffer &other);
    void shrink_to_fit();
    template <typename P>
    void append(const P *param);
    T *erase(T *ifirst, T*ilast);

protected:
    T *insert_common(T *where, size_t count);

private:
    static inline void destroy_range(T *first, T *last) {
        destroy_range_traits(first, last, detail::is_pod<T>());
    }

    static inline void move_urange(T *dest, T *first, T *last) {
        move_urange_traits(dest, first, last, detail::is_pod<T>());
    }

    static inline void bmove_urange(T *dest, T *first, T *last) {
        bmove_urange_traits(dest, first, last, detail::is_pod<T>());
    }

    static inline void fill_urange(T *first, T *last) {
        fill_urange_traits(first, last, detail::is_pod<T>());
    }

    static inline void fill_urange(T *first, T *last, const T &value) {
        fill_urange_traits(first, last, value, detail::is_pod<T>());
    }

    static inline T *reserve_traits(T *first, T *last, size_t capacity, detail::is_pod<T, false>) {
        T *memory = neoMalloc(sizeof *memory * capacity);
        move_urange(memory, first, last);
        neoFree(first);
        return memory;
    }

    static inline T *reserve_traits(T *first, T *last, size_t capacity, detail::is_pod<T, true>) {
        (void)last;
        return neoRealloc(first, sizeof(T) * capacity);
    }

    static inline T *reserve(T *first, T *last, size_t capacity) {
        return reserve_traits(first, last, capacity, detail::is_pod<T>());
    }

    static inline void destroy_range_traits(T *first, T *last, detail::is_pod<T, false>) {
        for (; first < last; ++first)
            first->~T();
    }

    static inline void destroy_range_traits(T *, T *, detail::is_pod<T, true>) {
        // Empty
    }

    static inline void fill_urange_traits(T *first, T *last, detail::is_pod<T, false>) {
        for (; first < last; ++first)
            new (first) T();
    }

    static inline void fill_urange_traits(T *first, T *last, detail::is_pod<T, true>) {
        for (; first < last; ++first)
             *first = T();
    }

    static inline void fill_urange_traits(T *first, T *last, const T &value, detail::is_pod<T, false>) {
        for (; first < last; ++first)
            new (first) T(value);
    }

    static inline void fill_urange_traits(T *first, T *last, const T &value, detail::is_pod<T, true>) {
        for (; first < last; ++first)
            *first = value;
    }

    static inline void move_urange_traits(T *dest, T *first, T *last, detail::is_pod<T, false>) {
        for (T *it = first; it != last; ++it, ++dest)
            new (dest) T(u::move(*it));
        destroy_range(first, last);
    }

    static inline void move_urange_traits(T *dest, T *first, T *last, detail::is_pod<T, true>) {
        for (; first != last; ++first, ++dest)
            *dest = *first;
    }

    static inline void bmove_urange_traits(T *dest, T *first, T *last, detail::is_pod<T, false>) {
        dest += (last - first);
        for (T *it = last; it != first; --it, --dest) {
            new (dest - 1) T(u::move(*(it - 1)));
            destroy_range(it - 1, it);
        }
    }

    static inline void bmove_urange_traits(T *dest, T *first, T *last, detail::is_pod<T, true>) {
        dest += (last - first);
        for (T *it = last; it != first; --it, --dest)
            *(dest - 1) = *(it - 1);
    }
};

template <typename T>
inline buffer<T>::buffer()
    : first(nullptr)
    , last(nullptr)
    , capacity(nullptr)
{
}

template <typename T>
inline buffer<T>::~buffer() {
    destroy_range(first, last);
    neoFree(first);
}

template <typename T>
inline buffer<T>::buffer(buffer<T> &&other) {
    first = other.first;
    last = other.last;
    capacity = other.capacity;
    other.first = nullptr;
    other.last = nullptr;
    other.capacity = nullptr;
}

template <typename T>
inline buffer<T> &buffer<T>::operator=(buffer<T> &&other) {
    assert(this != &other);
    destroy_range(first, last);
    neoFree(first);
    first = other.first;
    last = other.last;
    capacity = other.capacity;
    other.first = nullptr;
    other.last = nullptr;
    other.capacity = nullptr;
    return *this;
}

template <typename T>
inline void buffer<T>::destroy() {
    destroy_range(first, last);
    neoFree(first);
    first = nullptr;
    last = nullptr;
    capacity = nullptr;
}

template <typename T>
inline void buffer<T>::reserve(size_t icapacity) {
    if (first + icapacity <= capacity)
        return;

    T *resize = reserve(first, last, icapacity);
    last = resize + size_t(last - first);
    first = resize;
    capacity = resize + icapacity;
}

template <typename T>
inline void buffer<T>::resize(size_t size) {
    reserve(size);
    fill_urange(last, first + size);
    destroy_range(first + size, last);
    last = first + size;
}

template <typename T>
inline void buffer<T>::resize(size_t size, const T &value) {
    reserve(size);

    fill_urange(last, first + size, value);
    destroy_range(first + size, last);
    last = first + size;
}

template <typename T>
inline void buffer<T>::clear() {
    destroy_range(first, last);
    last = first;
}

template <typename T>
inline T *buffer<T>::insert_common(T *where, size_t count) {
    const size_t offset = size_t(where - first);
    const size_t newsize = size_t((last - first) + count);
    if (first + newsize > capacity)
        reserve((newsize * 3) / 2);
    where = first + offset;
    if (where != last)
        bmove_urange(where + count, where, last);
    last = first + newsize;
    return where;
}

template <typename T>
template <typename I>
inline void buffer<T>::insert(T *where, const I *ifirst, const I *ilast) {
    // Allow insertion ifirst/ilast to potentially overlap in memory
    using P = unsigned char *;
    const bool from = ((P)first <= (P)ifirst && (P)last >= (P)ilast);
    const size_t count = ilast - ifirst;
    size_t offset;
    if (from) {
        offset = (P)ifirst - (P)first;
        if ((P)where <= (P)ifirst)
            offset += count * sizeof(T);
    }
    where = insert_common(where, count);
    if (from) {
        ifirst = (I*)((P)first + offset);
        ilast = ifirst + count;
    }
    for (; ifirst != ilast; ++ifirst, ++where)
        new (where) T(*ifirst);
}

template <typename T>
inline void buffer<T>::insert(T *where, size_t count) {
    where = insert_common(where, count);
    for (T *end = where+count; where != end; ++where)
        new (where) T();
}

template <typename T>
inline void buffer<T>::swap(buffer<T> &other) {
    u::swap(first, other.first);
    u::swap(last, other.last);
    u::swap(capacity, other.capacity);
}

template <typename T>
inline void buffer<T>::shrink_to_fit() {
    if (last == first) {
        neoFree(first);
        capacity = first;
    } else if (capacity != last) {
        const size_t size = size_t(last - first);
        T *resize = neoMalloc(sizeof *resize * size);
        move_urange(resize, first, last);
        neoFree(first);
        first = resize;
        last = resize + size;
        capacity = last;
    }
}

template <typename T>
template <typename P>
inline void buffer<T>::append(const P *param) {
    if (capacity != last) {
        new (last) T(*param);
        ++last;
    } else {
        insert(last, param, param + 1);
    }
}

template <typename T>
inline T *buffer<T>::erase(T *ifirst, T*ilast) {
    const size_t range = ilast - ifirst;
    for (T *it = ilast, *end = last, *dest = ifirst; it != end; ++it, ++dest)
        *dest = u::move(*it);
    destroy_range(last - range, last);
    last -= range;
    return ifirst;
}

}
#endif
