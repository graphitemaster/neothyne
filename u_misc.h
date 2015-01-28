#ifndef U_MISC_HDR
#define U_MISC_HDR
#include <stdarg.h>  // va_start, va_end, va_list
#include <string.h>  // strcpy
#include <stdio.h>

#include "u_string.h" // u::string
#include "u_vector.h" // u::vector
#include "u_memory.h" // u::unique_ptr

namespace u {

template <typename T>
T endianSwap(T value) {
    union {
        int i;
        char data[sizeof(int)];
    } x = { 1 };
    if (x.data[0] != 1) {
        union {
            T value;
            unsigned char data[sizeof(T)];
        } src = { value }, dst;
        for (size_t i = 0; i < sizeof(T); i++)
            dst.data[i] = src.data[sizeof(T) - i - 1];
        return dst.value;
    }
    return value;
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

// argument normalization for format
template <typename T>
inline constexpr typename u::enable_if<u::is_integral<T>::value, long>::type
formatNormalize(T argument) { return argument; }

template <typename T>
inline constexpr typename u::enable_if<u::is_floating_point<T>::value, double>::type
formatNormalize(T argument) { return argument; }

template <typename T>
inline constexpr typename u::enable_if<u::is_pointer<T>::value, T>::type
formatNormalize(T argument) { return argument; }

inline const char *formatNormalize(const u::string &argument) {
    return argument.c_str();
}

static inline u::string formatProcess(const char *fmt, ...) {
    int n = strlen(fmt) * 2;
    int f = 0;
    u::string str;
    u::unique_ptr<char[]> formatted;
    va_list ap;
    for (;;) {
        formatted.reset(new char[n]);
        strcpy(&formatted[0], fmt);
        va_start(ap, fmt);
        f = vsnprintf(&formatted[0], n, fmt, ap);
        va_end(ap);
        if (f < 0 || f >= n)
            n += abs(f - n + 1);
        else
            break;
    }
    return string(formatted.get());
}

template <typename... Ts>
inline u::string format(const char *fmt, const Ts&... ts) {
    return formatProcess(fmt, formatNormalize(ts)...);
}

template <typename... Ts>
inline void fprint(FILE *fp, const char *fmt, const Ts&... ts) {
    fprintf(fp, "%s", format(fmt, ts...).c_str());
}

template <typename... Ts>
inline void print(const char *fmt, const Ts&... ts) {
    fprint(stdout, fmt, ts...);
    fflush(stdout);
}

}
#endif
