#ifndef U_FILE_HDR
#define U_FILE_HDR
#include <stdio.h>
#include "u_optional.h"
#include "util.h" // string

namespace u {
    // A little unique_ptr like file wrapper to achieve RAII. We can't use
    // unique_ptr here because unique_ptr doesn't allow null default delters
    struct file {
        file() :
            m_handle(nullptr) { }

        file(FILE *fp) :
            m_handle(fp) { }

        operator FILE*() {
            return m_handle;
        }

        FILE *get() {
            return m_handle;
        }

        ~file() {
            if (m_handle)
                fclose(m_handle);
        }

    private:
        FILE *m_handle;
    };

    // check if a file exists
    bool exists(const string &file);
    // open a file
    file fopen(const string& infile, const char *type);
    // read file line by line
    optional<string> getline(FILE *fp);
    // read a file into vector
    optional<vector<unsigned char>> read(const string &file, const char *mode = "rb");
    // write a vector to file
    bool write(const u::vector<unsigned char> &data, const string &file, const char *mode = "wb");
}
#endif
