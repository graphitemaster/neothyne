#ifndef UTIL_HDR
#define UTIL_HDR
#include <map>       // std::map
#include <set>       // std::set
#include <list>      // std::list
#include <vector>    // std::vector
#include <memory>    // std::unique_ptr
#include <algorithm> // std::sort
#include <string>    // std::string

#include <stdarg.h>  // va_start, va_end, va_list
#include <string.h>  // strcpy, strlen

namespace u {

template<typename T1, typename T2>
using map = std::map<T1, T2>;

template <typename T>
using list = std::list<T>;

template <typename T>
using set = std::set<T>;

template <typename T, typename D = std::default_delete<T>>
using unique_ptr = std::unique_ptr<T, D>;

template <typename T>
using vector = std::vector<T>;

using string = std::string;

template <typename I, typename T>
I find(I first, I last, const T &value) {
    while (first != last) {
        if (*first == value)
            return first;
        ++first;
    }
    return last;
}

// An implementation of std::move
template <typename T>
constexpr typename std::remove_reference<T>::type &&move(T &&t) {
    return static_cast<typename std::remove_reference<T>::type&&>(t);
}

template <typename T>
T min(const T &lhs, const T &rhs) {
    return lhs < rhs ? lhs : rhs;
}

template <typename T>
T max(const T &lhs, const T &rhs) {
    return lhs > rhs ? lhs : rhs;
}

template <typename T>
T abs(const T &lhs) {
    return lhs < 0 ? -lhs : lhs;
}

template <typename T>
void swap(T &lhs, T &rhs) {
    // use std here because their swap is more efficent than what we could
    // implement (memcpy for trivially copy assignable for instance)
    std::swap(lhs, rhs);
}

template <typename randomAccessIterator>
inline void sort(randomAccessIterator first, randomAccessIterator last) {
    // use std here because their sort is more efficent than what we could
    // implement (sorting network for small values, insertion sort, and then
    // heap sort (for libc++ for instance))
    std::sort(first, last);
}

inline int sscanf(const string &thing, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int value = vsscanf(thing.c_str(), fmt, va);
    va_end(va);
    return value;
}

inline u::string format(const string fmt, ...) {
    int n = int(fmt.size()) * 2;
    int f = 0;
    string str;
    unique_ptr<char[]> formatted;
    va_list ap;
    for (;;) {
        formatted.reset(new char[n]);
        strcpy(&formatted[0], fmt.c_str());
        va_start(ap, fmt);
        f = vsnprintf(&formatted[0], n, fmt.c_str(), ap);
        va_end(ap);
        if (f < 0 || f >= n)
            n += abs(f - n + 1);
        else
            break;
    }
    return string(formatted.get());
}

}

#endif
