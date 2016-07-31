#ifndef C_CONFIG_HDR
#define C_CONFIG_HDR

namespace u {

struct string;

}

namespace c {

struct Config {
    static bool read(const u::string &path);
    static bool write(const u::string &path);
};

}

#endif
