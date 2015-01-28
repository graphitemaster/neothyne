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
}

#endif
