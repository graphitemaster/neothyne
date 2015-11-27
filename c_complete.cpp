#include "u_memory.h"
#include "c_complete.h"

namespace c {

struct completer {
    completer();
    ~completer();
    completer *insert(const u::string &c, bool term = true);
    static int at(int ch);
    static completer *insert(completer *h, const u::string &c);
    static void dfs(completer *n, const u::string &data, u::vector<u::string> &matches);
    static void search(completer *n, const char *find, u::vector<u::string> &matches, const u::string &item = "");
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

inline completer *completer::insert(const u::string &c, bool term) {
    if (c.empty())
        return this;
    completer *p = this;
    for (auto &it : c) {
        const int i = at(it);
        assert(i != -1);
        if (!p->m_array[i])
            p->m_array[i] = new completer;
        p = p->m_array[i];
    }
    p->m_term = term;
    return this;
}

inline void completer::dfs(completer *n, const u::string &item, u::vector<u::string> &matches) {
    if (n) {
        if (n->m_term)
            matches.push_back(item);
        for (size_t i = 0; i < sizeof kAlphabet - 1; i++)
            dfs(n->m_array[i], item + kAlphabet[i], matches);
    }
}

inline completer *completer::insert(completer *h, const u::string &c) {
    return (h ? h : new completer)->insert(c, true);
}

inline void completer::search(completer *n, const char *s, u::vector<u::string> &matches, const u::string &item) {
    if (*s) {
        const int f = at(*s);
        if (f != -1 && n->m_array[f])
            search(n->m_array[f], s+1, matches, item + *s);
    } else {
        for (size_t i = 0; i < sizeof kAlphabet - 1; i++)
            dfs(n->m_array[i], item + kAlphabet[i], matches);
    }
}

void complete::insert(const char *ident) {
    gAutoComplete()->insert(gAutoComplete(), ident);
}

u::vector<u::string> complete::find(const char *prefix) {
    u::vector<u::string> matches;
    gAutoComplete()->search(gAutoComplete(), prefix, matches);
    return u::move(matches);
}

}
