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
    varReference()
        : desc(nullptr)
        , self(nullptr)
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
static inline varStatus varSet(const u::string &name, const T &value, bool callback) {
    if (variables().find(name) == variables().end())
        return kVarNotFoundError;
    auto &ref = variables()[name];
    if (ref.type != varTypeTraits<T>::type)
        return kVarTypeError;
    auto &val = *reinterpret_cast<var<T>*>(ref.self);
    varStatus status = val.set(value);
    if (status == kVarSuccess && callback)
        val();
    return status;
}

varStatus varChange(const u::string &name, const u::string &value, bool callback = false) {
    if (variables().find(name) == variables().end())
        return kVarNotFoundError;
    auto &ref = variables()[name];
    if (ref.type == kVarInt)
        return varSet<int>(name, u::atoi(value), callback);
    else if (ref.type == kVarFloat)
        return varSet<float>(name, u::atof(value), callback);
    else if (ref.type == kVarString) {
        u::string copy(value);
        copy.pop_front();
        copy.pop_back();
        return varSet<u::string>(name, copy, callback);
    }
    return kVarTypeError;
}

bool writeConfig() {
    u::file file = u::fopen(neoUserPath() + "init.cfg", "w");
    if (!file)
        return false;
    auto writeLine = [](FILE *fp, const u::string &name, const varReference &ref) {
        if (ref.type == kVarInt) {
            auto handle = reinterpret_cast<var<int>*>(ref.self);
            auto v = handle->get();
            if (handle->flags() & kVarPersist)
                fprintf(fp, "%s %d\n", name.c_str(), v);
        } else if (ref.type == kVarFloat) {
            auto handle = reinterpret_cast<var<float>*>(ref.self);
            auto v = handle->get();
            if (handle->flags() & kVarPersist)
                fprintf(fp, "%s %.2f\n", name.c_str(), v);
        } else if (ref.type == kVarString) {
            auto handle = reinterpret_cast<var<u::string>*>(ref.self);
            auto v = handle->get();
            if (handle->flags() & kVarPersist) {
                if (v.empty())
                    return;
                fprintf(fp, "%s \"%s\"\n", name.c_str(), v.c_str());
            }
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
        if (varChange(key, value) != kVarSuccess)
            return false;
    }
    return true;
}

}
