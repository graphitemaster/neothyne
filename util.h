#ifndef UTIL_HDR
#define UTIL_HDR
#include <map>
#include <vector>
#include <string>
#include <list>
#include <algorithm>

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

namespace u {
    template <typename T>
    using vector = std::vector<T>;
    using string = std::string;

    template<typename T1, typename T2>
    using map = std::map<T1, T2>;

    template <typename T>
    using list = std::list<T>;

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

    inline u::string format(const char *fmt, ...) {
        va_list va;
        va_start(va, fmt);
        char *data = nullptr;
        vasprintf(&data, fmt, va);
        va_end(va);
        u::string ret = data;
        free(data);
        return ret;
    }

    vector<unsigned char> compress(const vector<unsigned char> &data);
    vector<unsigned char> decompress(const vector<unsigned char> &data);
}
#endif
