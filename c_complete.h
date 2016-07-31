#ifndef C_COMPLETE_HDR
#define C_COMPLETE_HDR
#include "u_string.h"
#include "u_vector.h"

namespace c {

static constexpr char kAlphabet[] = "abcdefghijklmnopqrstuvwxyz_";

struct Complete {
    Complete();
    ~Complete();
    Complete *insert(const char *c, bool term = true);
    void search(const char *find, u::vector<u::string> &matches) const;
    void search(const u::string &find, u::vector<u::string> &matches) const;
    void add(const char *ident);
private:
    static int at(int ch);
    static Complete *insert(Complete *h, const char *c);
    static void dfs(const Complete *const n, u::string &&data, u::vector<u::string> &matches);
    static void search(const Complete *const n, const char *find, u::vector<u::string> &matches, const u::string &item = "");
    bool m_term;
    Complete *m_array[sizeof kAlphabet];
};

inline void Complete::search(const u::string &find, u::vector<u::string> &matches) const {
    return search(&find[0], matches);
}

}

#endif
