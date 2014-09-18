#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "u_file.h"

namespace u {
    bool exists(const string &file) {
        return u::fopen(file, "r").get();
    }

    u::file fopen(const string& infile, const char *type) {
        string file = infile;
        // Deal with constructing the directories if the file contains a path
        for (size_t i = 0; file[i]; i++) {
            if (i > 0 && (file[i] == '\\' || file[i] == '/')) {
                char slash = file[i];
                file[i] = '\0';
                if (mkdir(file.c_str(), 0777) != 0 && errno != EEXIST)
                    return nullptr;
                file[i] = slash;
            }
        }
        return ::fopen(infile.c_str(), type);
    }

    optional<string> getline(FILE *fp) {
        string s;
        for (;;) {
            char buf[256];
            if (!fgets(buf, sizeof(buf), fp)) {
                if (feof(fp)) {
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

    optional<vector<unsigned char>> read(const string &file, const char *mode) {
        auto fp = u::fopen(file, mode);
        if (!fp)
            return u::none;

        vector<unsigned char> data;
        fseek(fp.get(), 0, SEEK_END);
        data.resize(ftell(fp.get()));
        fseek(fp.get(), 0, SEEK_SET);

        if (fread(&data[0], data.size(), 1, fp.get()) != 1)
            return u::none;
        return data;
    }

    bool write(const u::vector<unsigned char> &data, const string &file, const char *mode) {
        auto fp = u::fopen(file, mode);
        if (!fp)
            return false;
        fwrite(&data[0], data.size(), 1, fp.get());
        return true;
    }
}
