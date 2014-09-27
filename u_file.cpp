#include <errno.h>
#include <stdlib.h>

#ifndef _WIN32
#   include <sys/stat.h>
#   include <sys/types.h>
#else
#   include <direct.h>
#endif

#include "u_file.h"
#include "u_traits.h"

namespace u {

bool exists(const u::string &file) {
    return u::fopen(file, "r").get();
}

bool mkdir(const u::string &dir) {
#ifdef _WIN32
    return ::_mkdir(dir.c_str()) == 0;
#else
    return ::mkdir(dir.c_str(), 0755) == 0;
#endif
}

u::file fopen(const u::string& infile, const char *type) {
    u::string file = infile;
    // Deal with constructing the directories if the file contains a path
    for (size_t i = 0; file[i]; i++) {
        if (i > 0 && (file[i] == '\\' || file[i] == '/')) {
            char slash = file[i];
            file[i] = '\0';
            if (!u::mkdir(file) && errno != EEXIST)
                return nullptr;
            file[i] = slash;
        }
    }
    return ::fopen(infile.c_str(), type);
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
    fwrite(&data[0], data.size(), 1, fp.get());
    return true;
}

}
