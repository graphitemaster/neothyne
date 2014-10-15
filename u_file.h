#ifndef U_FILE_HDR
#define U_FILE_HDR
#include <stdio.h>

#include "u_optional.h"
#include "u_string.h"
#include "u_vector.h"

namespace u {

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

// check if a file exists
bool exists(const u::string &file);
// remove a file
bool remove(const u::string &file);
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
