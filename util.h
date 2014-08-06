#ifndef UTIL_HDR
#define UTIL_HDR
#include <vector>
#include <algorithm>
#include <string>

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

namespace u {
    template <typename T>
    using vector = std::vector<T>;
    using string = std::string;

    template <typename randomAccessIterator>
    inline void sort(randomAccessIterator first, randomAccessIterator last) {
        std::sort(first, last);
    }

    inline int getline(u::string &store, FILE *fp) {
        char *data = nullptr;
        size_t size = 0;
        int value = getline(&data, &size, fp);
        store = data;
        free(data);
        return value;
    }

    inline FILE *fopen(const string& file, const char *type) {
        return ::fopen(file.c_str(), type);
    }

    inline int sscanf(const u::string &thing, const char *fmt, ...) {
        va_list va;
        va_start(va, fmt);
        int value = vsscanf(thing.c_str(), fmt, va);
        va_end(va);
        return value;
    }
}
#endif
