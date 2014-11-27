#ifndef U_STACK_HDR
#define U_STACK_HDR
#include <stddef.h>

namespace u {

template <typename T, size_t E>
struct stack {
    stack();
    const T *begin() const;
    const T *end() const;
    T &next();
    void push_back(const T &data);
    const T &operator[](size_t index) const;
    T &operator[](size_t index);
    T pop_back();
    void reset();
    size_t size() const;
    bool full() const;
private:
    T m_data[E];
    size_t m_size;
};

template <typename T, size_t E>
inline stack<T, E>::stack()
    : m_size(0)
{
}

template <typename T, size_t E>
inline const T *stack<T, E>::begin() const {
    return &m_data[0];
}

template <typename T, size_t E>
inline const T *stack<T, E>::end() const {
    return &m_data[m_size];
}

template <typename T, size_t E>
inline T &stack<T, E>::next() {
    return m_data[m_size++];
}

template <typename T, size_t E>
inline void stack<T, E>::push_back(const T &data) {
    if (full()) reset();
    m_data[m_size++] = data;
}

template <typename T, size_t E>
inline const T &stack<T, E>::operator[](size_t index) const {
    return m_data[index];
}

template <typename T, size_t E>
inline T &stack<T, E>::operator[](size_t index) {
    return m_data[index];
}

template <typename T, size_t E>
inline T stack<T, E>::pop_back() {
    return m_data[m_size--];
}

template <typename T, size_t E>
inline void stack<T, E>::reset() {
    m_size = 0;
}

template <typename T, size_t E>
inline size_t stack<T, E>::size() const {
    return m_size;
}

template <typename T, size_t E>
inline bool stack<T, E>::full() const {
    return m_size >= E;
}

}

#endif
