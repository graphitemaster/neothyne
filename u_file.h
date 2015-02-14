#ifndef U_FILE_HDR
#define U_FILE_HDR
#include <stdio.h>

#include "u_optional.h"
#include "u_string.h"
#include "u_vector.h"

namespace u {

#ifdef _WIN32
static constexpr int kPathSep = '\\';
#else
static constexpr int kPathSep = '/';
#endif

// A little unique_ptr like file wrapper to achieve RAII. We can't use
// unique_ptr here because unique_ptr doesn't allow null default delters
struct file {
    file();
    file(FILE *fp);
    ~file();

    operator FILE*();
    FILE *get();

private:
    FILE *m_handle;
};

enum pathType {
    kFile,
    kDirectory
};

struct dir {
    struct const_iterator {
        const_iterator(void *handle);
        const_iterator& operator++();
        const char *operator*() const;

        friend bool operator !=(const const_iterator &a, const const_iterator &);

    private:
        void *m_handle;
        const char *m_name;
    };

    dir(const u::string &where);
    dir(const char *where);
    ~dir();

    const_iterator begin() const;
    const_iterator end() const;

    static bool isFile(const char *fileName);
    static bool isFile(const u::string &fileName);

private:
    void *m_handle;
};

// check if a file or directory exists
bool exists(const u::string &path, pathType type = kFile);
// remove a file or directory
bool remove(const u::string &file, pathType type = kFile);

// open a file
u::file fopen(const u::string& infile, const char *type);
// read file line by line
u::optional<u::string> getline(u::file &fp);
// read a file into vector
u::optional<u::vector<unsigned char>> read(const u::string &file, const char *mode = "rb");
// write a vector to file
bool write(const u::vector<unsigned char> &data, const u::string &file, const char *mode = "wb");
// make a directory
bool mkdir(const u::string &dir);

///! dir
inline dir::dir(const u::string &where)
    : dir(where.c_str())
{
}

inline const char *dir::const_iterator::operator*() const {
    return m_name ? m_name : "";
}

inline bool operator !=(const dir::const_iterator &a, const dir::const_iterator &) {
    return a.m_name;
}

inline dir::const_iterator dir::begin() const {
    return { m_handle };
}

inline dir::const_iterator dir::end() const {
    return { nullptr };
}

inline bool dir::isFile(const u::string &fileName) {
    return dir::isFile(fileName.c_str());
}

}

#endif
