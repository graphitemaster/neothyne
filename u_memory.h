#ifndef U_MEMORY_HDR
#define U_MEMORY_HDR
#include "u_traits.h"

namespace u {

typedef decltype(nullptr) nullptr_t;

/// A basic pair for unique_ptr (we don't use u::pair here)
template <typename T1, typename T2>
struct basic_pair {
    constexpr basic_pair();
    constexpr basic_pair(T1 f, T2 s);
    T1 first;
    T2 second;
};

template <typename T1, typename T2>
inline constexpr basic_pair<T1, T2>::basic_pair()
    : first()
    , second()
{
}

template <typename T1, typename T2>
inline constexpr basic_pair<T1, T2>::basic_pair(T1 f, T2 s)
    : first(forward<T1>(f))
    , second(forward<T2>(s))
{
}

/// default_delete<T>
template <typename T>
struct default_delete {
    constexpr default_delete();
    template<typename O>
    default_delete(const default_delete<O>&);
    void operator()(T *data);
};

/// default_delete<T[]>
template <typename T>
struct default_delete<T[]> {
    constexpr default_delete();
    template<typename O>
    default_delete(const default_delete<O>&);
    void operator()(T *data);
    template<typename O>
    void operator()(O *data) = delete;
};

/// unique_ptr<T>
template <typename T, typename D = default_delete<T>>
struct unique_ptr {
    typedef T element_type;
    typedef element_type* pointer;
    typedef element_type& reference;
    typedef D deleter_type;
    typedef basic_pair<pointer, D> pair_type;

    pointer get() const;
    const D &get_deleter() const;

    unique_ptr();
    unique_ptr(nullptr_t);
    explicit unique_ptr(pointer data);
    unique_ptr(unique_ptr &&o);
    template <typename OT, typename OD>
    unique_ptr(unique_ptr<OT, OD> &&o);
    ~unique_ptr();

    pointer release();
    void reset(nullptr_t);
    void reset(pointer v);
    void swap(unique_ptr &&o);
    operator bool() const;
    reference operator*() const;
    pointer operator->() const;
    reference operator[](size_t i) const;

    unique_ptr &operator=(unique_ptr &&o);
    unique_ptr &operator=(nullptr_t);

    template <typename OT, typename OD>
    unique_ptr &operator=(unique_ptr<OT,OD> &&o);

private:
    D &deleter();
    void kill();
    pair_type m_data;
};

template<typename T, typename D>
struct unique_ptr<T[], D> {
    typedef T element_type[];
    typedef T* pointer;
    typedef T& reference;
    typedef D deleter_type;
    typedef basic_pair<T*,D> pair_type;

    pointer get() const;
    const D& get_deleter() const;

    unique_ptr();
    unique_ptr(nullptr_t);
    explicit unique_ptr(pointer data);
    unique_ptr(unique_ptr &&o);
    template <typename OT, typename OD>
    unique_ptr(unique_ptr<OT, OD> &&o);
    ~unique_ptr();

    pointer release();

    void reset(nullptr_t);
    void reset(pointer v);
    void swap(unique_ptr &&o);

    operator bool() const;
    reference operator*() const;
    pointer operator->() const;
    reference operator[](size_t i) const;

    unique_ptr &operator=(unique_ptr &&o);
    unique_ptr &operator=(nullptr_t);
    template<typename OT, typename OD>
    unique_ptr &operator=(unique_ptr<OT,OD> &&o);

private:
    D& deleter();
    void kill();
    pair_type m_data;
};

/// default_delete<T>
template <typename T>
inline constexpr default_delete<T>::default_delete() = default;

template <typename T>
template <typename O>
inline default_delete<T>::default_delete(const default_delete<O> &) {
    // Do nothing
}

template <typename T>
inline void default_delete<T>::operator()(T *data) {
    delete data;
}

/// default_delete<T[]>
template <typename T>
inline constexpr default_delete<T[]>::default_delete() = default;

template <typename T>
template <typename O>
inline default_delete<T[]>::default_delete(const default_delete<O> &) {
    // Do nothing
}

template <typename T>
inline void default_delete<T[]>::operator()(T *data) {
    delete[] data;
}

/// unique_ptr<T, D>
template <typename T, typename D>
inline typename unique_ptr<T, D>::pointer unique_ptr<T, D>::get() const {
    return m_data.first;
}

template <typename T, typename D>
inline const D &unique_ptr<T, D>::get_deleter() const {
    return m_data.second;
}

template <typename T, typename D>
inline unique_ptr<T, D>::unique_ptr()
    : m_data(nullptr, D())
{
}

template <typename T, typename D>
inline unique_ptr<T, D>::unique_ptr(nullptr_t)
    : unique_ptr()
{
}

template <typename T, typename D>
inline unique_ptr<T, D>::unique_ptr(pointer data)
    : m_data(data, D())
{
}

template <typename T, typename D>
inline unique_ptr<T, D>::unique_ptr(unique_ptr &&o)
    : m_data(move(o.m_data))
{
    o.m_data.first = nullptr;
}

template <typename T, typename D>
template <typename OT, typename OD>
inline unique_ptr<T, D>::unique_ptr(unique_ptr<OT, OD> &&o)
    : m_data(move(o.first), D())
{
    o.m_data.first = nullptr;
}

template <typename T, typename D>
inline unique_ptr<T, D>::~unique_ptr() {
    kill();
}

template <typename T, typename D>
inline typename unique_ptr<T, D>::pointer unique_ptr<T, D>::release() {
    pointer v = get();
    m_data.first = nullptr;
    return v;
}

template <typename T, typename D>
inline void unique_ptr<T, D>::reset(nullptr_t) {
    kill();
}

template <typename T, typename D>
inline void unique_ptr<T, D>::reset(pointer v) {
    kill();
    m_data.first = v;
}

template <typename T, typename D>
inline void unique_ptr<T, D>::swap(unique_ptr &&o) {
    pair_type tmp(move(m_data));
    m_data = move(o.m_data);
    o.m_data = move(tmp);
}

template <typename T, typename D>
inline unique_ptr<T, D>::operator bool() const {
    return m_data.first;
}

template <typename T, typename D>
inline typename unique_ptr<T, D>::reference unique_ptr<T, D>::operator*() const {
    return *m_data.first;
}

template <typename T, typename D>
inline typename unique_ptr<T, D>::pointer unique_ptr<T, D>::operator->() const {
    return m_data.first;
}

template <typename T, typename D>
inline typename unique_ptr<T, D>::reference unique_ptr<T, D>::operator[](size_t i) const {
    return m_data.first[i];
}

template <typename T, typename D>
inline unique_ptr<T, D> &unique_ptr<T, D>::operator=(unique_ptr &&o) {
    kill();
    m_data = move(o.m_data);
    o.m_data.first = nullptr;
    return *this;
}

template <typename T, typename D>
inline unique_ptr<T, D> &unique_ptr<T, D>::operator=(nullptr_t) {
    reset();
    return *this;
}

template <typename T, typename D>
template <typename OT, typename OD>
inline unique_ptr<T, D> &unique_ptr<T, D>::operator=(unique_ptr<OT,OD> &&o) {
    kill();
    m_data.first = move(o.m_data.first);
    o.m_data.first = nullptr;
    return *this;
}

template <typename T, typename D>
inline D &unique_ptr<T, D>::deleter() {
    return m_data.second;
}

template <typename T, typename D>
inline void unique_ptr<T, D>::kill() {
    if (m_data.first) {
        deleter()(m_data.first);
        m_data.first = nullptr;
    }
}

/// unique_ptr<T[], D>
template <typename T, typename D>
inline typename unique_ptr<T[], D>::pointer unique_ptr<T[], D>::get() const {
    return &*m_data.first;
}

template <typename T, typename D>
inline const D& unique_ptr<T[], D>::get_deleter() const {
    return m_data.second;
}

template <typename T, typename D>
inline unique_ptr<T[], D>::unique_ptr()
    : m_data(nullptr, D())
{
}

template <typename T, typename D>
inline unique_ptr<T[], D>::unique_ptr(nullptr_t)
    : unique_ptr()
{
}

template <typename T, typename D>
inline unique_ptr<T[], D>::unique_ptr(pointer data)
    : m_data(data, D())
{
}

template <typename T, typename D>
inline unique_ptr<T[], D>::unique_ptr(unique_ptr &&o)
    : m_data(move(o.m_data))
{
    o.m_data.first = nullptr;
}

template <typename T, typename D>
template <typename OT, typename OD>
inline unique_ptr<T[], D>::unique_ptr(unique_ptr<OT, OD> &&o)
    : m_data(move(o.first), D())
{
    o.m_data.first = nullptr;
}

template <typename T, typename D>
inline unique_ptr<T[], D>::~unique_ptr() {
    kill();
}

template <typename T, typename D>
inline typename unique_ptr<T[], D>::pointer unique_ptr<T[], D>::release() {
    pointer v = get();
    m_data.first = nullptr;
    return v;
}

template <typename T, typename D>
inline void unique_ptr<T[], D>::reset(nullptr_t) {
    kill();
}

template <typename T, typename D>
inline void unique_ptr<T[], D>::reset(pointer v) {
    kill();
    m_data.first = v;
}

template <typename T, typename D>
inline void unique_ptr<T[], D>::swap(unique_ptr &&o) {
    pair_type tmp(move(m_data));
    m_data = move(o.m_data);
    o.m_data = move(tmp);
}

template <typename T, typename D>
inline unique_ptr<T[], D>::operator bool() const {
    return m_data.first;
}

template <typename T, typename D>
inline typename unique_ptr<T[], D>::reference unique_ptr<T[], D>::operator*() const {
    return *m_data.first;
}

template <typename T, typename D>
inline typename unique_ptr<T[], D>::pointer unique_ptr<T[], D>::operator->() const {
    return m_data.first;
}

template <typename T, typename D>
inline typename unique_ptr<T[], D>::reference unique_ptr<T[], D>::operator[](size_t i) const {
    return m_data.first[i];
}

template <typename T, typename D>
inline unique_ptr<T[], D> &unique_ptr<T[], D>::operator=(unique_ptr &&o) {
    kill();
    m_data = move(o.m_data);
    o.m_data.first = nullptr;
    return *this;
}

template <typename T, typename D>
inline unique_ptr<T[], D> &unique_ptr<T[], D>::operator=(nullptr_t) {
    reset();
    return *this;
}

template <typename T, typename D>
template <typename OT, typename OD>
inline unique_ptr<T[], D> &unique_ptr<T[], D>::operator=(unique_ptr<OT,OD> &&o) {
    kill();
    m_data.first = move(o.m_data.first);
    o.m_data.first = nullptr;
    return *this;
}

template <typename T, typename D>
inline D &unique_ptr<T[], D>::deleter() {
    return m_data.second;
}

template <typename T, typename D>
inline void unique_ptr<T[], D>::kill() {
    if (m_data.first) {
        deleter()(m_data.first);
        m_data.first = nullptr;
    }
}

///! deferred_data
template <typename T, bool F = true>
struct deferred_data {
    deferred_data() = default;
    ~deferred_data();
    T *operator()();
    void free();
private:
    unsigned char m_data[sizeof(T)];
};

template <typename T, bool F>
inline deferred_data<T, F>::~deferred_data() {
    if (F) free();
}

template <typename T, bool F>
inline T *deferred_data<T, F>::operator()() {
    void *p = m_data;
    static bool initialized = false;
    if (initialized)
        return (T*)p;
    initialized = true;
    return new ((T*)p) T();
}

template <typename T, bool F>
inline void deferred_data<T, F>::free() {
    void *p = m_data;
    ((T*)p)->~T();
}

}

#endif
