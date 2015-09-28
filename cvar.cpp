#include <assert.h>

#include "engine.h"
#include "cvar.h"

#include "u_file.h"
#include "u_memory.h"

struct varReference;

struct autoComplete {
    autoComplete();
    ~autoComplete();
    autoComplete *insert(const u::string &c, bool term = true);
    static int at(int ch);
    static autoComplete *insert(autoComplete *h, const u::string &c);
    static void dfs(autoComplete *n, const u::string &data, u::vector<u::string> &matches);
    static void search(autoComplete *n, const char *find, u::vector<u::string> &matches, const u::string &item = "");
private:
    static constexpr char kAlphabet[] = "abcdefghijklmnopqrstuvwxyz_";
    bool m_term;
    autoComplete *m_array[sizeof(kAlphabet)];
};

constexpr char autoComplete::kAlphabet[];

inline autoComplete::autoComplete()
    : m_term(false)
{
    for (auto &it : m_array)
        it = nullptr;
}

inline autoComplete::~autoComplete() {
    for (auto &it : m_array)
        delete it;
}

inline int autoComplete::at(int ch) {
    const char *find = strchr(kAlphabet, ch);
    return find ? find - kAlphabet : -1;
}

inline autoComplete *autoComplete::insert(const u::string &c, bool term) {
    if (c.empty())
        return this;
    autoComplete *p = this;
    for (auto &it : c) {
        const int i = at(it);
        assert(i != -1);
        if (!p->m_array[i])
            p->m_array[i] = new autoComplete;
        p = p->m_array[i];
    }
    p->m_term = term;
    return this;
}

inline void autoComplete::dfs(autoComplete *n, const u::string &item, u::vector<u::string> &matches) {
    if (n) {
        if (n->m_term)
            matches.push_back(item);
        for (size_t i = 0; i < sizeof(kAlphabet) - 1; i++)
            dfs(n->m_array[i], item + kAlphabet[i], matches);
    }
}

inline autoComplete *autoComplete::insert(autoComplete *h, const u::string &c) {
    return (h ? h : new autoComplete)->insert(c, true);
}

inline void autoComplete::search(autoComplete *n, const char *s, u::vector<u::string> &matches, const u::string &item) {
    if (*s) {
        const int f = at(*s);
        if (f != -1 && n->m_array[f])
            search(n->m_array[f], s+1, matches, item + *s);
    } else {
        for (size_t i = 0; i < sizeof(kAlphabet) - 1; i++)
            dfs(n->m_array[i], item + kAlphabet[i], matches);
    }
}

struct varReference {
    varReference()
        : desc(nullptr)
        , self(nullptr)
        , type(kVarInt)
    {
    }

    varReference(const char *desc, void *self, varType type)
        : desc(desc)
        , self(self)
        , type(type)
    {
    }

    const char *desc;
    void *self;
    varType type;
};

static u::deferred_data<autoComplete> gAutoComplete;
static u::deferred_data<u::map<u::string, varReference>> gVariables;

// public API
void varRegister(const char *name, const char *desc, void *self, varType type) {
    if (gVariables()->find(name) != gVariables()->end())
        return;
    (*gVariables())[name] = varReference(desc, self, type);
    autoComplete::insert(gAutoComplete(), name);
}

template <typename T>
var<T> &varGet(const char *name) {
    auto &ref = (*gVariables())[name];
    auto &val = *(var<T>*)(ref.self);
    return val;
}

template var<int> &varGet<int>(const char *name);
template var<float> &varGet<float>(const char *name);

u::optional<u::string> varValue(const u::string &name) {
    if (gVariables()->find(name) == gVariables()->end())
        return u::none;

    const auto &ref = (*gVariables())[name];

    switch (ref.type) {
    case kVarFloat:
        return u::format("%.2f", ((var<float>*)ref.self)->get());
    case kVarInt:
        return u::format("%d", ((var<int>*)ref.self)->get());
    case kVarString:
        return u::format("%s", ((var<u::string>*)ref.self)->get());
    }

    return u::none;
}

template <typename T>
static inline varStatus varSet(const u::string &name, const T &value, bool callback) {
    if (gVariables()->find(name) == gVariables()->end())
        return kVarNotFoundError;
    const auto &ref = (*gVariables())[name];
    if (ref.type != varTypeTraits<T>::type)
        return kVarTypeError;
    auto &val = *(var<T>*)(ref.self);
    varStatus status = val.set(value);
    if (status == kVarSuccess && callback)
        val();
    return status;
}

varStatus varChange(const u::string &name, const u::string &value, bool callback) {
    if (gVariables()->find(name) == gVariables()->end())
        return kVarNotFoundError;
    const auto &ref = (*gVariables())[name];
    if (ref.type == kVarInt) {
        for (int it : value)
            if (!strchr("0123456789", it))
                return kVarTypeError;
        return varSet<int>(name, u::atoi(value), callback);
    } else if (ref.type == kVarFloat) {
        float val = 0.0f;
        if (u::sscanf(value, "%f", &val) != 1)
            return kVarTypeError;
        return varSet<float>(name, val, callback);
    } else if (ref.type == kVarString) {
        u::string copy(value);
        copy.pop_front();
        copy.pop_back();
        return varSet<u::string>(name, u::move(copy), callback);
    }
    return kVarTypeError;
}

u::vector<u::string> varAutoComplete(const u::string &what) {
    u::vector<u::string> matches;
    autoComplete::search(gAutoComplete(), what.c_str(), matches);
    return u::move(matches);
}

bool writeConfig(const u::string &userPath) {
    u::file file = u::fopen(userPath + "init.cfg", "w");
    if (!file)
        return false;
    auto writeLine = [](FILE *fp, const u::string &name, const varReference &ref) {
        if (ref.type == kVarInt) {
            auto handle = (var<int>*)ref.self;
            const auto &v = handle->get();
            if (handle->flags() & kVarPersist)
                u::fprint(fp, "%s %d\n", name, v);
        } else if (ref.type == kVarFloat) {
            auto handle = (var<float>*)ref.self;
            const auto &v = handle->get();
            if (handle->flags() & kVarPersist)
                u::fprint(fp, "%s %.2f\n", name, v);
        } else if (ref.type == kVarString) {
            auto handle = (var<u::string>*)ref.self;
            const auto &v = handle->get();
            if (handle->flags() & kVarPersist) {
                if (v.empty())
                    return;
                u::fprint(fp, "%s \"%s\"\n", name, v.c_str());
            }
        }
    };
    for (const auto &it : *gVariables())
        writeLine(file, it.first, it.second);
    return true;
}

bool readConfig(const u::string &userPath) {
    u::file file = u::fopen(userPath + "init.cfg", "r");
    if (!file)
        return false;
    while (auto getline = u::getline(file)) {
        const u::string &line = *getline;
        u::vector<u::string> kv = u::split(line);
        if (kv.size() != 2)
            continue;
        const u::string &key = kv[0];
        const u::string &value = kv[1];
        if (varChange(key, value) != kVarSuccess)
            return false;
    }
    return true;
}
