#include <stdlib.h>

#include "engine.h"

#include "c_var.h"

#include "u_file.h"
#include "u_vector.h"
#include "u_misc.h"
#include "u_string.h"

namespace c {

struct varReference;
static u::map<u::string, varReference> *vars = nullptr;

struct varReference {
    varReference() { }
    varReference(const char *desc, void *self, varType type) :
        desc(desc),
        self(self),
        type(type)
    {
    }

    const char *desc;
    void *self;
    varType type;
};

static u::map<u::string, varReference> &variables() {
    if (vars)
        return *vars;
    return *(vars = new u::map<u::string, varReference>());
}

// public API
void varRegister(const char *name, const char *desc, void *self, varType type) {
    if (variables().find(name) != variables().end())
        return;
    variables()[name] = varReference(desc, self, type);
}

template <typename T>
static inline void varSet(const u::string &name, const T &value, bool callback) {
    if (variables().find(name) == variables().end())
        return;
    auto &ref = variables()[name];
    if (ref.type != varTypeTraits<T>::type)
        return;
    auto &val = *reinterpret_cast<var<T>*>(ref.self);
    val.set(value);
    if (callback)
        val();
}

void varChange(const u::string &name, const u::string &value, bool callback = false) {
    if (variables().find(name) == variables().end())
        return;
    auto &ref = variables()[name];
    if (ref.type == VAR_INT)
        varSet<int>(name, u::atoi(value), callback);
    else if (ref.type == VAR_FLOAT)
        varSet<float>(name, u::atof(value), callback);
    else if (ref.type == VAR_STRING) {
        u::string copy(value);
        copy.pop_front();
        copy.pop_back();
        varSet<u::string>(name, copy, callback);
    }
}

bool writeConfig() {
    u::file file = u::fopen(neoUserPath() + "init.cfg", "w");
    if (!file)
        return false;
    auto writeLine = [](FILE *fp, const u::string &name, const varReference &ref) {
        if (ref.type == VAR_INT) {
            auto v = reinterpret_cast<var<int>*>(ref.self)->get();
            fprintf(fp, "%s %d\n", name.c_str(), v);
        } else if (ref.type == VAR_FLOAT) {
            auto v = reinterpret_cast<var<float>*>(ref.self)->get();
            fprintf(fp, "%s %.2f\n", name.c_str(), v);
        } else if (ref.type == VAR_STRING) {
            auto &v = reinterpret_cast<var<u::string>*>(ref.self)->get();
            if (v.empty())
                return;
            fprintf(fp, "%s \"%s\"\n", name.c_str(), v.c_str());
        }
    };
    for (auto &it : variables())
        writeLine(file, it.first, it.second);
    return true;
}

bool readConfig() {
    u::file file = u::fopen(neoUserPath() + "init.cfg", "r");
    if (!file)
        return false;
    while (auto getline = u::getline(file)) {
        u::string& line = *getline;
        u::vector<u::string> kv = u::split(line);
        if (kv.size() != 2)
            continue;
        const u::string &key = kv[0];
        const u::string &value = kv[1];
        varChange(key, value);
    }
    return true;
}

}
