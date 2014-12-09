#include <errno.h>
#include <stdlib.h>
#include <string.h>

// These exist on Windows too
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#   include <direct.h>
#else
#   include <unistd.h> // rmdir
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
bool exists(const u::string &path, pathType type) {
    if (type == kFile) {
        // fixPath not called since u::fopen fixes the path
        return u::fopen(path, "rb").get();
    }

    // type == kDirectory
    struct stat info;
    if (stat(fixPath(path).c_str(), &info) != 0)
        return false; // Couldn't stat directory
    if (!(info.st_mode & S_IFDIR)) // S_ISDIR not suported everywhere
        return false; // Not a directory
    return true;
}

bool remove(const u::string &path, pathType type) {
    u::string fix = fixPath(path);
    if (type == kFile)
        return ::remove(fix.c_str()) == 0;

    // type == kDirectory
#ifdef _WIN32
    return ::_rmdir(fix.c_str()) == 0;
#else
    return ::rmdir(fix.c_str()) == 0;
#endif
}

bool mkdir(const u::string &dir) {
    u::string fix = fixPath(dir);
    const char *path = fix.c_str();
#ifdef _WIN32
    return ::_mkdir(path) == 0;
#else
    return ::mkdir(path, 0775);
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
    auto size = ftell(fp.get());
    if (size <= 0)
        return u::none;
    data.resize(size);
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
