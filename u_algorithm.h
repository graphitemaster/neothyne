#ifndef U_ALGORITHM_HDR
#define U_ALGORITHM_HDR
#include <algorithm> // std::sort

namespace u {

template <typename T>
constexpr typename std::remove_reference<T>::type &&move(T &&t) {
    return static_cast<typename std::remove_reference<T>::type&&>(t);
}

template <typename randomAccessIterator>
inline void sort(randomAccessIterator first, randomAccessIterator last) {
    // use std here because their sort is more efficent than what we could
    // implement (sorting network for small values, insertion sort, and then
    // heap sort (for libc++ for instance))
    std::sort(first, last);
}

template <typename T>
inline void swap(T &lhs, T &rhs) {
    T temp = move(rhs);
    rhs = move(lhs);
    lhs = move(temp);
}

template <typename I, typename T>
inline I find(I first, I last, const T &value) {
    while (first != last) {
        if (*first == value)
            return first;
        ++first;
    }
    return last;
}

template <typename T>
inline T min(const T &lhs, const T &rhs) {
    return lhs < rhs ? lhs : rhs;
}

template <typename T>
inline T max(const T &lhs, const T &rhs) {
    return lhs > rhs ? lhs : rhs;
}

template <typename T>
inline T abs(const T &lhs) {
    return lhs < 0 ? -lhs : lhs;
}

}

#endif
