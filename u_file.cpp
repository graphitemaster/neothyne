#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#   define _WIN32_LEAN_AND_MEAN
#   define NOMINMAX
#   include <windows.h>
#else
#    include <sys/stat.h>   // S_ISREG, stat
#   include <dirent.h>  // opendir, readir, DIR
#   include <unistd.h>  // rmdir, mkdir
#endif

#include "u_file.h"
#include "u_algorithm.h"
#include "u_misc.h"

namespace u {

file::file()
    : m_handle(nullptr)
{
}

file::file(FILE *fp)
    : m_handle(fp)
{
}

file::file(file &&other)
    : m_handle(other.m_handle)
{
    other.m_handle = nullptr;
}

file::~file() {
    if (m_handle)
        fclose(m_handle);
}

file &file::operator=(file &&other) {
    m_handle = other.m_handle;
    other.m_handle = nullptr;
    return *this;
}

file::operator FILE*() {
    return m_handle;
}

FILE *file::get() const {
    return m_handle;
}

void file::close() {
    fclose(m_handle);
    m_handle = nullptr;
}

u::string fixPath(const u::string &path) {
    u::string fix = path;
    for (char *it = &fix[0]; (it = strpbrk(it, "/\\")); *it++ = u::kPathSep)
        ;
    return fix;
}

// file system stuff
bool exists(const u::string &inputPath, pathType type) {
    u::string &&path = u::move(u::fixPath(inputPath));
    if (type == kFile)
        return dir::isFile(path);

    // type == kDirectory
#if defined(_WIN32)
    const DWORD attribs = GetFileAttributesA(inputPath.c_str());
    if (attribs == INVALID_FILE_ATTRIBUTES)
        return false;
    if (!(attribs & FILE_ATTRIBUTE_DIRECTORY))
        return false;
#else
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
        return false; // Couldn't stat directory
    if (!(info.st_mode & S_IFDIR))
        return false; // Not a directory
#endif
    return true;
}

bool remove(const u::string &path, pathType type) {
    u::string &&fix = u::move(fixPath(path));
    if (type == kFile) {
#if defined(_WIN32)
        return DeleteFileA(&fix[0]) != 0;
#else
        return ::remove(&fix[0]) == 0;
#endif
    }

    // type == kDirectory
#if defined(_WIN32)
    return RemoveDirectoryA(&fix[0]) != 0;
#else
    return ::rmdir(&fix[0]) == 0;
#endif
}

bool mkdir(const u::string &dir) {
    u::string &&fix = u::move(u::fixPath(dir));
#if defined(_WIN32)
    return CreateDirectoryA(&fix[0], nullptr) != 0;
#else
    return ::mkdir(&fix[0], 0775);
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
        if (n && buf[n - 1] == '\r')
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
    if (fseek(fp.get(), 0, SEEK_END) != 0)
        return u::none;
    const auto size = ftell(fp.get());
    if (size <= 0)
        return u::none;
    data.resize(size);
    if (fseek(fp.get(), 0, SEEK_SET) != 0)
        return u::none;
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

#define IS_IGNORE(X) ((X)->d_name[0] == '.' && !(X)->d_name[1+((X)->d_name[1]=='.')])

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
#define IS_IGNORE(X) ((X).cFileName[0] == '.' && !(X).cFileName[1+((X).cFileName[1]=='.')])

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
    U_ASSERT(length + sizeof kPathExtra < MAX_PATH);
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
    const DWORD attribs = GetFileAttributesA(fileName);
    if (attribs == INVALID_FILE_ATTRIBUTES)
        return false;
    if (attribs & FILE_ATTRIBUTE_DIRECTORY)
        return false;
    return true;
}
#endif

}
