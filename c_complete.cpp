#include "u_memory.h"
#include "c_complete.h"

namespace c {

static constexpr char kAlphabet[] = "abcdefghijklmnopqrstuvwxyz_";

///! Completer
struct Completer {
    Completer();
    ~Completer();
    Completer *insert(const char *c, bool term = true);
    void insert(const char *c);
    static int at(int ch);
    static Completer *insert(Completer *h, const char *c);
    static void dfs(const Completer *const n, u::string &&data, u::vector<u::string> &matches);
    static void search(const Completer *const n, const char *find, u::vector<u::string> &matches, const u::string &item = "");
    void search(const char *find, u::vector<u::string> &matches) const;
private:
    bool m_term;
    Completer *m_array[sizeof kAlphabet];
};

static u::deferred_data<Completer> gAutoComplete;

inline Completer::Completer()
    : m_term(false)
{
    for (auto &it : m_array)
        it = nullptr;
}

inline Completer::~Completer() {
    for (auto &it : m_array)
        delete it;
}

inline int Completer::at(int ch) {
    const char *find = strchr(kAlphabet, ch);
    return find ? find - kAlphabet : -1;
}

inline Completer *Completer::insert(const char *c, bool term) {
    if (!*c)
        return this;
    Completer *p = this;
    for (const char *it = c; *it; ++it) {
        const int i = at(*it);
        U_ASSERT(i != -1);
        if (!p->m_array[i])
            p->m_array[i] = new Completer;
        p = p->m_array[i];
    }
    p->m_term = term;
    return this;
}

inline void Completer::dfs(const Completer *const n, u::string &&item, u::vector<u::string> &matches) {
    if (n) {
        if (n->m_term)
            matches.push_back(item);
        for (size_t i = 0; i < sizeof kAlphabet - 1; i++)
            dfs(n->m_array[i], item + kAlphabet[i], matches);
    }
}

inline void Completer::insert(const char *c) {
    Completer::insert(this, c);
}

inline Completer *Completer::insert(Completer *h, const char *c) {
    return (h ? h : new Completer)->insert(c, true);
}

inline void Completer::search(const Completer *const n, const char *s, u::vector<u::string> &matches, const u::string &item) {
    if (*s) {
        const int f = at(*s);
        if (f != -1 && n->m_array[f])
            search(n->m_array[f], s+1, matches, item + *s);
    } else {
        for (size_t i = 0; i < sizeof kAlphabet - 1; i++)
            dfs(n->m_array[i], u::move(item + kAlphabet[i]), matches);
    }
}

inline void Completer::search(const char *s, u::vector<u::string> &matches) const {
    Completer::search(this, s, matches);
}

///! Complete
void Complete::insert(const char *ident) {
    gAutoComplete()->insert(gAutoComplete(), ident);
}

u::vector<u::string> Complete::find(const char *prefix) {
    u::vector<u::string> matches;
    gAutoComplete()->search(prefix, matches);
    return u::move(matches);
}

}
