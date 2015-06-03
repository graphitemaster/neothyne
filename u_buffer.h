#ifndef U_BUFFER_HDR
#define U_BUFFER_HDR
#include <stdlib.h>

#include "u_new.h"
#include "u_traits.h"

namespace u {

namespace detail {
    template <typename T, bool pod = u::is_pod<T>::value>
    struct is_pod { };

    template <typename T, T value>
    struct swap_test;

    template <typename T>
    static inline void move_construct(T *a, T &b, ...) {
        new(a) T(b);
    }

    template <typename T>
    static inline void move_construct(T *a, T &b, void *,
        swap_test<void (T::*)(T&), &T::swap> * = 0)
    {
        new(a) T(b);
        a->swap(b);
    }

    template <typename T>
    static inline void move_impl(T &a, T &b, ...) {
        a = b;
    }

    template <typename T>
    static inline void move_impl(T &a, T &b, T*, swap_test<void (T::*)(T&), &T::swap>* = 0) {
        a.swap(b);
    }

    template <typename T>
    static inline void move(T &a, T &b) {
        move_impl(a, b, (T*)0);
    }
}

template <typename T>
static inline void move_construct(T *a, T& b) {
    detail::move_construct(a, b, (T*)0);
}

template <typename T>
struct buffer {
    buffer();
    ~buffer();

    T *first;
    T *last;
    T *capacity;

    void destroy();
    void destroy_range(T *first, T *last);
    void move_urange(T *dest, T *first, T *last);
    void bmove_urange(T *dest, T *first, T *last);
    void fill_urange(T *first, T *last, const T &value);
    void resize(size_t size, const T &value);
    void reserve(size_t icapacity);
    void clear();
    template <typename I>
    void insert(T *where, const I *ifirst, const I *ilast);
    void swap(buffer &other);

    T *erase(T *ifirst, T*ilast);

private:
    void reserve_traits(size_t icapacity, detail::is_pod<T, false>);
    void reserve_traits(size_t icapacity, detail::is_pod<T, true>);
    void destroy_range_traits(T *first, T *last, detail::is_pod<T, false>);
    void destroy_range_traits(T*, T*, detail::is_pod<T, true>);
    void fill_urange_traits(T *first, T *last, const T &value,
        detail::is_pod<T, false>);
    void fill_urange_traits(T *first, T *last, const T &value,
        detail::is_pod<T, true>);
    void move_urange_traits(T *dest, T *first, T *last,
        detail::is_pod<T, false>);
    void move_urange_traits(T *dest, T *first, T *last,
        detail::is_pod<T, true>);
    void bmove_urange_traits(T *dest, T *first, T *last,
        detail::is_pod<T, false>);
    void bmove_urange_traits(T *dest, T *first, T *last,
        detail::is_pod<T, true>);
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
inline void buffer<T>::destroy_range(T *first, T *last) {
    destroy_range_traits(first, last, detail::is_pod<T>());
}

template <typename T>
inline void buffer<T>::move_urange(T *dest, T *first, T *last) {
    move_urange_traits(dest, first, last, detail::is_pod<T>());
}

template <typename T>
inline void buffer<T>::bmove_urange(T *dest, T *first, T *last) {
    bmove_urange_traits(dest, first, last, detail::is_pod<T>());
}

template <typename T>
inline void buffer<T>::fill_urange(T *first, T *last, const T &value) {
    fill_urange_traits(first, last, value, detail::is_pod<T>());
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
    reserve_traits(icapacity, detail::is_pod<T>());
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
template <typename I>
inline void buffer<T>::insert(T *where, const I *ifirst, const I *ilast) {
    const size_t offset = size_t(where - first);
    const size_t newsize = size_t((last - first) + (ilast - ifirst));
    if (first + newsize > capacity)
        reserve((newsize * 3) / 2);
    where = first + offset;
    const size_t count = size_t(ilast - ifirst);
    if (where != last)
        bmove_urange(where + count, where, last);
    for (; ifirst != ilast; ++ifirst, ++where)
        new(where) T(*ifirst);
    last = first + newsize;
}

template <typename T>
inline void buffer<T>::swap(buffer<T> &other) {
    T *tfirst = first;
    T *tlast = last;
    T *tcapacity = capacity;
    first = other.first;
    last = other.last;
    capacity = other.capacity;
    other.first = tfirst;
    other.last = tlast;
    other.capacity = tcapacity;
}

template <typename T>
inline void buffer<T>::reserve_traits(size_t icapacity, detail::is_pod<T, false>) {
    if (first + icapacity <= capacity)
        return;

    T *newfirst = neoMalloc(sizeof(T) * icapacity);
    const size_t size = size_t(last - first);
    move_urange(newfirst, first, last);
    neoFree(first);

    first = newfirst;
    last = newfirst + size;
    capacity = newfirst + icapacity;
}

template <typename T>
inline void buffer<T>::reserve_traits(size_t icapacity, detail::is_pod<T, true>) {
    if (first + icapacity <= capacity)
        return;

    T *newfirst = neoRealloc(first, sizeof(T) * icapacity);
    const size_t size = size_t(last - first);
    first = newfirst;
    last = newfirst + size;
    capacity = newfirst + icapacity;
}

template <typename T>
inline void buffer<T>::destroy_range_traits(T *first, T *last, detail::is_pod<T, false>) {
    for (; first < last; ++first)
        first->~T();
}

template <typename T>
inline void buffer<T>::destroy_range_traits(T*, T*, detail::is_pod<T, true>) {
    // Empty
}

template <typename T>
inline void buffer<T>::fill_urange_traits(T *first, T *last, const T &value,
    detail::is_pod<T, false>)
{
    for (; first < last; ++first)
        new(first) T(value);
}

template <typename T>
inline void buffer<T>::fill_urange_traits(T *first, T *last, const T &value,
    detail::is_pod<T, true>)
{
    for (; first < last; ++first)
        *first = value;
}

template <typename T>
inline void buffer<T>::move_urange_traits(T *dest, T *first, T *last,
    detail::is_pod<T, false>)
{
    for (T *it = first; it != last; ++it, ++dest)
        move_construct(dest, *it);
    destroy_range(first, last);
}

template <typename T>
inline void buffer<T>::move_urange_traits(T *dest, T *first, T *last,
    detail::is_pod<T, true>)
{
    for (; first != last; ++first, ++dest)
        *dest = *first;
}

template <typename T>
inline void buffer<T>::bmove_urange_traits(T *dest, T *first, T *last,
    detail::is_pod<T, false>)
{
    dest += (last - first);
    for (T *it = last; it != first; --it, --dest) {
        move_construct(dest - 1, *(it - 1));
        destroy_range(it - 1, it);
    }
}

template <typename T>
inline void buffer<T>::bmove_urange_traits(T *dest, T *first, T *last,
    detail::is_pod<T, true>)
{
    dest += (last - first);
    for (T* it = last; it != first; --it, --dest)
        *(dest - 1) = *(it - 1);
}

template <typename T>
inline T *buffer<T>::erase(T *ifirst, T*ilast) {
    const size_t range = ilast - ifirst;
    for (T *it = ilast, *end = last, *dest = ifirst; it != end; ++it, ++dest)
        detail::move(*dest, *it);
    destroy_range(last - range, last);
    last -= range;
    return ifirst;
}

}
#endif
