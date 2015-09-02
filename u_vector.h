#ifndef U_VECTOR_HDR
#define U_VECTOR_HDR
#include "u_buffer.h"

// Must be in namespace std for the compiler to proxy it
namespace std {

template <typename T>
struct initializer_list {
    typedef T value_type;
    typedef const T &reference;
    typedef const T &const_reference;
    typedef size_t size_type;
    typedef const T *iterator;
    typedef const T *const_iterator;

    initializer_list();

    size_t size() const;
    const T* begin() const;
    const T* end() const;

    const T &operator[](size_t index) const;

private:
    initializer_list(const T* data, size_t size);
    const T *m_begin;
    size_t m_size;
};

template <typename T>
inline initializer_list<T>::initializer_list()
    : m_begin(nullptr)
    , m_size(0)
{
}

template <typename T>
inline size_t initializer_list<T>::size() const {
    return m_size;
}

template <typename T>
inline const T *initializer_list<T>::begin() const {
    return m_begin;
}

template <typename T>
inline const T *initializer_list<T>::end() const {
    return m_begin + m_size;
}

template <typename T>
inline const T &initializer_list<T>::operator[](size_t index) const {
    return *(m_begin + index);
}

template <typename T>
inline initializer_list<T>::initializer_list(const T* data, size_t size)
    : m_begin(data)
    , m_size(size)
{
}

}

namespace u {

template <typename T>
using initializer_list = std::initializer_list<T>;

template <typename T>
struct vector {
    typedef T value_type;
    typedef T* iterator;
    typedef const T* const_iterator;

    vector();
    vector(const vector &other);
    vector(vector &&other);
    vector(size_t size);
    vector(size_t size, const T &value);
    vector(const T *first, const T *last);
    vector(initializer_list<T> data);
    ~vector();

    vector &operator=(const vector &other);
    vector &operator=(vector &&other);

    void assign(const T *first, const T *last);

    const T *data() const;
    T *data();
    size_t size() const;
    size_t capacity() const;
    bool empty() const;

    T &operator[](size_t idx);
    const T &operator[](size_t idx) const;

    const T &back() const;
    T &back();

    void resize(size_t size);
    void resize(size_t size, const T &value);
    void clear();
    void reserve(size_t capacity);

    void push_back(const T &t);
    void pop_back();

    void swap(vector& other);
    void destroy();

    void insert(iterator where, const T &value);
    void shrink_to_fit();

    template <typename I>
    void insert(iterator where, const I *first, const I *last);

    iterator begin();
    iterator end();

    iterator erase(iterator first, iterator last);
    iterator erase(iterator position);

    const_iterator begin() const;
    const_iterator end() const;

private:
    buffer<T> m_buffer;
};

template <typename T>
inline vector<T>::vector() {
    // Empty
}

template <typename T>
inline vector<T>::vector(const vector &other) {
    m_buffer.reserve(other.size());
    m_buffer.insert(m_buffer.last, other.m_buffer.first, other.m_buffer.last);
}

template <typename T>
inline vector<T>::vector(vector &&other)
    : m_buffer(u::move(other.m_buffer))
{
    // Empty
}

template <typename T>
inline vector<T>::vector(size_t size) {
    m_buffer.resize(size, T());
}

template <typename T>
inline vector<T>::vector(size_t size, const T &value) {
    m_buffer.resize(size, value);
}

template <typename T>
inline vector<T>::vector(const T *first, const T *last) {
    m_buffer.insert(m_buffer.last, first, last);
}

template <typename T>
inline vector<T>::vector(initializer_list<T> data) {
    m_buffer.insert(m_buffer.last, data.begin(), data.end());
}

template <typename T>
inline vector<T>::~vector() {
    // Empty
}

template <typename T>
inline vector<T> &vector<T>::operator=(const vector<T> &other) {
    vector(other).swap(*this);
    return *this;
}

template <typename T>
inline vector<T> &vector<T>::operator=(vector<T> &&other) {
    m_buffer = u::move(other.m_buffer);
    return *this;
}

template <typename T>
inline void vector<T>::assign(const T *first, const T *last) {
    m_buffer.clear();
    m_buffer.insert(m_buffer.last, first, last);
}

template <typename T>
inline const T* vector<T>::data() const {
    return m_buffer.first;
}

template <typename T>
inline T* vector<T>::data() {
    return m_buffer.first;
}

template <typename T>
inline size_t vector<T>::size() const {
    return size_t(m_buffer.last - m_buffer.first);
}

template <typename T>
inline size_t vector<T>::capacity() const {
    return size_t(m_buffer.capacity - m_buffer.first);
}

template <typename T>
inline bool vector<T>::empty() const {
    return m_buffer.last == m_buffer.first;
}

template <typename T>
inline T &vector<T>::operator[](size_t idx) {
    return m_buffer.first[idx];
}

template <typename T>
inline const T &vector<T>::operator[](size_t idx) const {
    return m_buffer.first[idx];
}

template <typename T>
inline const T &vector<T>::back() const {
    return m_buffer.last[-1];
}

template <typename T>
inline T &vector<T>::back() {
    return m_buffer.last[-1];
}

template <typename T>
inline void vector<T>::resize(size_t size) {
    m_buffer.resize(size, T());
}

template <typename T>
inline void vector<T>::resize(size_t size, const T &value) {
    m_buffer.resize(size, value);
}

template <typename T>
inline void vector<T>::clear() {
    m_buffer.clear();
}

template <typename T>
inline void vector<T>::reserve(size_t capacity) {
    m_buffer.reserve(capacity);
}

template <typename T>
inline void vector<T>::push_back(const T &value) {
    m_buffer.insert(m_buffer.last, &value, &value + 1);
}

template <typename T>
inline void vector<T>::pop_back() {
    m_buffer.erase(m_buffer.last - 1, m_buffer.last);
}

template <typename T>
inline void vector<T>::swap(vector<T> &other) {
    m_buffer.swap(other.m_buffer);
}

template <typename T>
inline void vector<T>::destroy() {
    m_buffer.destroy();
}

template <typename T>
inline typename vector<T>::iterator vector<T>::begin() {
    return m_buffer.first;
}

template <typename T>
inline typename vector<T>::iterator vector<T>::end() {
    return m_buffer.last;
}

template <typename T>
inline typename vector<T>::const_iterator vector<T>::begin() const {
    return m_buffer.first;
}

template <typename T>
inline typename vector<T>::const_iterator vector<T>::end() const {
    return m_buffer.last;
}

template <typename T>
inline typename vector<T>::iterator vector<T>::erase(iterator first, iterator last) {
    return m_buffer.erase(first, last);
}

template <typename T>
inline typename vector<T>::iterator vector<T>::erase(iterator position) {
    return m_buffer.erase(position, position + 1);
}

template <typename T>
inline void vector<T>::insert(iterator where, const T &value) {
    m_buffer.insert(where, &value, &value + 1);
}

template <typename T>
inline void vector<T>::shrink_to_fit() {
    m_buffer.shrink_to_fit();
}

template <typename T>
template <typename I>
inline void vector<T>::insert(iterator where, const I *first, const I *last) {
    m_buffer.insert(where, first, last);
}

}

#endif
