#include "u_memory.h"
#include "c_complete.h"

namespace c {

struct completer {
    completer();
    ~completer();
    completer *insert(const char *c, bool term = true);
    void insert(const char *c);
    static int at(int ch);
    static completer *insert(completer *h, const char *c);
    static void dfs(const completer *const n, u::string &&data, u::vector<u::string> &matches);
    static void search(const completer *const n, const char *find, u::vector<u::string> &matches, const u::string &item = "");
    void search(const char *find, u::vector<u::string> &matches) const;
private:
    static constexpr char kAlphabet[] = "abcdefghijklmnopqrstuvwxyz_";
    bool m_term;
    completer *m_array[sizeof kAlphabet];
};

constexpr char completer::kAlphabet[];

static u::deferred_data<completer> gAutoComplete;

inline completer::completer()
    : m_term(false)
{
    for (auto &it : m_array)
        it = nullptr;
}

inline completer::~completer() {
    for (auto &it : m_array)
        delete it;
}

inline int completer::at(int ch) {
    const char *find = strchr(kAlphabet, ch);
    return find ? find - kAlphabet : -1;
}

inline completer *completer::insert(const char *c, bool term) {
    if (!*c)
        return this;
    completer *p = this;
    for (const char *it = c; *it; ++it) {
        const int i = at(*it);
        u::assert(i != -1);
        if (!p->m_array[i])
            p->m_array[i] = new completer;
        p = p->m_array[i];
    }
    p->m_term = term;
    return this;
}

inline void completer::dfs(const completer *const n, u::string &&item, u::vector<u::string> &matches) {
    if (n) {
        if (n->m_term)
            matches.push_back(item);
        for (size_t i = 0; i < sizeof kAlphabet - 1; i++)
            dfs(n->m_array[i], item + kAlphabet[i], matches);
    }
}

inline void completer::insert(const char *c) {
    completer::insert(this, c);
}

inline completer *completer::insert(completer *h, const char *c) {
    return (h ? h : new completer)->insert(c, true);
}

inline void completer::search(const completer *const n, const char *s, u::vector<u::string> &matches, const u::string &item) {
    if (*s) {
        const int f = at(*s);
        if (f != -1 && n->m_array[f])
            search(n->m_array[f], s+1, matches, item + *s);
    } else {
        for (size_t i = 0; i < sizeof kAlphabet - 1; i++)
            dfs(n->m_array[i], u::move(item + kAlphabet[i]), matches);
    }
}

inline void completer::search(const char *s, u::vector<u::string> &matches) const {
    completer::search(this, s, matches);
}

void complete::insert(const char *ident) {
    gAutoComplete()->insert(gAutoComplete(), ident);
}

u::vector<u::string> complete::find(const char *prefix) {
    u::vector<u::string> matches;
    gAutoComplete()->search(prefix, matches);
    return u::move(matches);
}

}
