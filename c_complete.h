#ifndef C_COMPLETE_HDR
#define C_COMPLETE_HDR
#include "u_string.h"
#include "u_vector.h"

namespace c {

struct Complete {
    static void insert(const char *ident);
    static void insert(const u::string &ident);
    static u::vector<u::string> find(const char *prefix);
    static u::vector<u::string> find(const u::string &prefix);
};

inline void Complete::insert(const u::string &ident) {
    Complete::insert(ident.c_str());
}

inline u::vector<u::string> Complete::find(const u::string &ident) {
    return Complete::find(ident.c_str());
}

}

#endif
