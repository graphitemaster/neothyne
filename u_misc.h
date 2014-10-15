#ifndef U_MISC_HDR
#define U_MISC_HDR
#include <stdarg.h>  // va_start, va_end, va_list
#include <string.h>  // strcpy

#include "u_string.h" // u::string
#include "u_vector.h" // u::vector
#include "u_memory.h" // u::unique_ptr

namespace u {

// Simple hash function via SDBM
namespace detail {
    static inline size_t sdbm(const void *data, size_t length) {
        size_t hash = 0;
        unsigned char *as = (unsigned char *)data;
        for (unsigned char *it = as, *end = as + length; it != end; ++it)
            hash = *it + (hash << 6) + (hash << 16) - hash;
        return hash;
    }
}

template <typename T>
inline size_t hash(const T &value) {
    const size_t rep = size_t(value);
    return detail::sdbm((const void *)&rep, sizeof(rep));
}

static inline size_t hash(const string &value) {
    return detail::sdbm(value.c_str(), value.size());
}

inline int sscanf(const u::string &thing, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int value = vsscanf(thing.c_str(), fmt, va);
    va_end(va);
    return value;
}

inline u::vector<u::string> split(const char *str, char ch = ' ') {
    u::vector<u::string> result;
    do {
        const char *begin = str;
        while (*str != ch && *str)
            str++;
        result.push_back(u::string(begin, str));
    } while (*str++);
    return result;
}

inline u::vector<u::string> split(const u::string &str, char ch = ' ') {
    return u::split(str.c_str(), ch);
}

inline int atoi(const u::string &str) {
    return ::atoi(str.c_str());
}

inline float atof(const u::string &str) {
    return ::atof(str.c_str());
}

inline u::string format(const u::string fmt, ...) {
    int n = int(fmt.size()) * 2;
    int f = 0;
    u::string str;
    u::unique_ptr<char[]> formatted;
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
