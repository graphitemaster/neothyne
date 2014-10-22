#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#   include <sys/stat.h>
#   include <sys/types.h>
#else
#   include <direct.h>
#endif

#include "u_file.h"
#include "u_algorithm.h"

namespace u {

file::file()
    : m_handle(nullptr)
{
}

file::file(FILE *fp)
    : m_handle(fp)
{
}

file::~file() {
    if (m_handle)
        fclose(m_handle);
}


file::operator FILE*() {
    return m_handle;
}

FILE *file::get() {
    return m_handle;
}

static inline u::string fixPath(const u::string &path) {
#ifdef _WIN32
    u::string fix(path);
    const size_t size = fix.size();
    for (size_t i = 0; i < size; i++) {
        if (fix[i] != '/')
            continue;
        fix[i] = PATH_SEP;
    }
    return fix;
#endif
    return path;
}

// file system stuff
bool exists(const u::string &file) {
    // fixPath not called since fopen calls it
    return u::fopen(file, "rb").get();
}

bool remove(const u::string &file) {
    return remove(fixPath(file).c_str()) == 0;
}

bool mkdir(const u::string &dir) {
    const char *path = fixPath(dir).c_str();
#ifdef _WIN32
    return ::_mkdir(path) == 0;
#else
    return ::mkdir(path, 0755) == 0;
#endif
}

u::file fopen(const u::string& infile, const char *type) {
    return ::fopen(fixPath(infile).c_str(), type);
}

u::optional<u::string> getline(u::file &fp) {
    u::string s;
    for (;;) {
        char buf[256];
        if (!fgets(buf, sizeof(buf), fp.get())) {
            if (feof(fp.get())) {
                if (s.empty())
                    return u::none;
                else
                    return u::move(s);
            }
            abort();
        }
        size_t n = strlen(buf);
        if (n && buf[n - 1] == '\n')
            --n;
        s.append(buf, n);
        if (n < sizeof(buf) - 1)
            return u::move(s);
    }
}

u::optional<u::vector<unsigned char>> read(const u::string &file, const char *mode) {
    auto fp = u::fopen(file, mode);
    if (!fp)
        return u::none;

    u::vector<unsigned char> data;
    fseek(fp.get(), 0, SEEK_END);
    data.resize(ftell(fp.get()));
    fseek(fp.get(), 0, SEEK_SET);

    if (fread(&data[0], data.size(), 1, fp.get()) != 1)
        return u::none;
    return data;
}

bool write(const u::vector<unsigned char> &data, const u::string &file, const char *mode) {
    auto fp = u::fopen(file, mode);
    if (!fp)
        return false;
    if (fwrite(&data[0], data.size(), 1, fp.get()) != 1)
        return false;
    return true;
}

}
