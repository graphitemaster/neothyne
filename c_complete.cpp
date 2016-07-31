#include "u_memory.h"
#include "c_complete.h"

namespace c {

Complete::Complete()
    : m_term(false)
{
    for (auto &it : m_array)
        it = nullptr;
}

Complete::~Complete() {
    for (auto &it : m_array)
        delete it;
}

int Complete::at(int ch) {
    const char *find = strchr(kAlphabet, ch);
    return find ? find - kAlphabet : -1;
}

Complete *Complete::insert(const char *c, bool term) {
    if (!*c)
        return this;
    Complete *p = this;
    for (const char *it = c; *it; ++it) {
        const int i = at(*it);
        U_ASSERT(i != -1);
        if (!p->m_array[i])
            p->m_array[i] = new Complete;
        p = p->m_array[i];
    }
    p->m_term = term;
    return this;
}

void Complete::dfs(const Complete *const n, u::string &&item, u::vector<u::string> &matches) {
    if (n) {
        if (n->m_term)
            matches.push_back(item);
        for (size_t i = 0; i < sizeof kAlphabet - 1; i++)
            dfs(n->m_array[i], item + kAlphabet[i], matches);
    }
}

void Complete::add(const char *ident) {
    Complete::insert(this, ident);
}

Complete *Complete::insert(Complete *h, const char *c) {
    return (h ? h : new Complete)->insert(c, true);
}

void Complete::search(const Complete *const n, const char *s, u::vector<u::string> &matches, const u::string &item) {
    if (*s) {
        const int f = at(*s);
        if (f != -1 && n->m_array[f])
            search(n->m_array[f], s+1, matches, item + *s);
    } else {
        for (size_t i = 0; i < sizeof kAlphabet - 1; i++)
            dfs(n->m_array[i], u::move(item + kAlphabet[i]), matches);
    }
}

void Complete::search(const char *s, u::vector<u::string> &matches) const {
    Complete::search(this, s, matches);
}

}
