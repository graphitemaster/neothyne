#ifndef UTIL_HDR
#define UTIL_HDR
#include <map>       // std::map
#include <set>       // std::set
#include <vector>    // std::vector
#include <string>    // std::string
#include <memory>    // std::unique_ptr

#include <stdarg.h>  // va_start, va_end, va_list
#include <string.h>  // strcpy, strlen

namespace u {

template<typename T1, typename T2>
using map = std::map<T1, T2>;

template <typename T>
using set = std::set<T>;

template <typename T>
using vector = std::vector<T>;

using string = std::string;

template <typename T>
using unique_ptr = std::unique_ptr<T, std::default_delete<T>>;

inline int sscanf(const string &thing, const char *fmt, ...) {
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
