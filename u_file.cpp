#include <stdlib.h>
#include <string.h>

// These exist on Windows too
#include <sys/stat.h>   // S_ISREG, stat
#include <sys/types.h>

#if defined(_WIN32)
#   define _WIN32_LEAN_AND_MEAN
#   define NOMINMAX
#   include <windows.h>
#   include <direct.h>  // rmdir, mkdir
#else
#   include <dirent.h>  // opendir, readir, DIR
#   include <unistd.h>  // rmdir, mkdir
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

u::string fixPath(const u::string &path) {
    u::string fix = path;
    for (auto &it : fix)
        if (strchr("/\\", it))
            it = u::kPathSep;
    return fix;
}

// file system stuff
bool exists(const u::string &path, pathType type) {
    if (type == kFile)
        return dir::isFile(path);

    // type == kDirectory
#if !defined(_WIN32)
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
        return false; // Couldn't stat directory
    if (!(info.st_mode & S_IFDIR))
        return false; // Not a directory
#else
    // _stat does not like trailing path separators
    u::string strip = path;
    if (strip.end()[-1] == u::kPathSep)
        strip.pop_back();
    struct _stat info;
    if (_stat(strip.c_str(), &info) != 0)
        return false;
#if !defined(S_IFDIR)
#  define S_IFDIR _S_IFDIR
#endif
    if (!(info.st_mode & _S_IFDIR))
        return false;
#endif
    return true;
}

bool remove(const u::string &path, pathType type) {
    u::string fix = fixPath(path);
    if (type == kFile)
        return ::remove(fix.c_str()) == 0;

    // type == kDirectory
#if defined(_WIN32)
    return ::_rmdir(fix.c_str()) == 0;
#else
    return ::rmdir(fix.c_str()) == 0;
#endif
}

bool mkdir(const u::string &dir) {
    u::string fix = fixPath(dir);
    const char *path = fix.c_str();
#if defined(_WIN32)
    return ::_mkdir(path) == 0;
#else
    return ::mkdir(path, 0775);
#endif
}

u::file fopen(const u::string& infile, const char *type) {
    return ::fopen(fixPath(infile).c_str(), type);
}

bool getline(u::file &fp, u::string &line) {
    line.clear();
    for (;;) {
        char buf[256];
        if (!fgets(buf, sizeof buf, fp.get())) {
            if (feof(fp.get()))
                return !line.empty();
            abort();
        }
        size_t n = strlen(buf);
        if (n && buf[n - 1] == '\n')
            --n;
        line.append(buf, n);
        if (n < sizeof buf - 1)
            return true;
    }
    return false;
}

u::optional<u::string> getline(u::file &fp) {
    u::string s;
    if (!getline(fp, s))
        return u::none;
    return u::move(s);
}

u::optional<u::vector<unsigned char>> read(const u::string &file, const char *mode) {
    auto fp = u::fopen(file, mode);
    if (!fp)
        return u::none;

    u::vector<unsigned char> data;
    fseek(fp.get(), 0, SEEK_END);
    const auto size = ftell(fp.get());
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

///! dir
#if !defined(_WIN32)
#include <unistd.h>
#include <dirent.h>

#define IS_IGNORE(X) (!strcmp((X)->d_name, ".") || !strcmp((X)->d_name, ".."))

///! dir::const_iterator
dir::const_iterator::const_iterator(void *handle)
    : m_handle(handle)
    , m_name(nullptr)
{
    if (!m_handle)
        return;
    // Ignore "." and ".."
    struct dirent *next = readdir((DIR *)m_handle);
    while (next && IS_IGNORE(next))
        next = readdir((DIR *)m_handle);
    m_name = next ? next->d_name : nullptr;
}

dir::const_iterator &dir::const_iterator::operator++() {
    struct dirent *next = readdir((DIR *)m_handle);
    while (next && IS_IGNORE(next))
        next = readdir((DIR *)m_handle);
    m_name = next ? next->d_name : nullptr;
    return *this;
}

///! dir
dir::dir(const char *where)
    : m_handle((void *)opendir(where))
{
}

dir::~dir() {
    if (m_handle)
        closedir((DIR *)m_handle);
}

bool dir::isFile(const char *fileName) {
    struct stat buff;
    if (stat(fileName, &buff) != 0)
        return false;
    return S_ISREG(buff.st_mode);
}

#else
#define IS_IGNORE(X) (!strcmp((X).cFileName, ".") || !strcmp((X).cFileName, ".."))

struct findContext {
    findContext(const char *where);
    ~findContext();
    HANDLE handle;
    WIN32_FIND_DATA findData;
};

inline findContext::findContext(const char *where)
    : handle(INVALID_HANDLE_VALUE)
{
    static constexpr const char kPathExtra[] = "\\*";
    const size_t length = strlen(where);
    u::assert(length + sizeof kPathExtra < MAX_PATH);
    char path[MAX_PATH];
    memcpy((void *)path, (const void *)where, length);
    memcpy((void *)&path[length], (const void *)kPathExtra, sizeof kPathExtra);
    if (!(handle = FindFirstFileA(path, &findData)))
        return;
    // Ignore "." and ".."
    if (IS_IGNORE(findData)) {
        BOOL next = FindNextFileA(handle, &findData);
        while (next && IS_IGNORE(findData))
            next = FindNextFileA(handle, &findData);
    }
}

inline findContext::~findContext() {
    if (handle != INVALID_HANDLE_VALUE)
        FindClose(handle);
}

///! dir::const_iterator
dir::const_iterator::const_iterator(void *handle)
    : m_handle(handle)
    , m_name(((findContext*)m_handle)->findData.cFileName)
{
}

dir::const_iterator &dir::const_iterator::operator++() {
    findContext *context = (findContext*)m_handle;
    BOOL next = FindNextFileA(context->handle, &context->findData);
    // Ignore "." and ".."
    while (next && IS_IGNORE(context->findData))
        next = FindNextFileA(context->handle, &context->findData);
    m_name = next ? context->findData.cFileName : nullptr;
    return *this;
}

///! dir
dir::dir(const char *where)
    : m_handle(new findContext(where))
{
}

dir::~dir() {
    delete (findContext*)m_handle;
}

bool dir::isFile(const char *fileName) {
    struct _stat buff;
    if (_stat(fileName, &buff) != 0)
        return false;
#if defined(S_ISREG)
    return S_ISREG(buff.st_mode);
#else
    return _S_ISREG(buff.st_mode);
#endif
}
#endif

}
