#include "engine.h"
#include "cvar.h"

#include "u_file.h"

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
var<T> &varGet(const char *name) {
    auto &ref = variables()[name];
    auto &val = *(var<T>*)(ref.self);
    return val;
}

template var<int> &varGet<int>(const char *name);
template var<float> &varGet<float>(const char *name);

template <typename T>
static inline varStatus varSet(const u::string &name, const T &value, bool callback) {
    if (variables().find(name) == variables().end())
        return kVarNotFoundError;
    auto &ref = variables()[name];
    if (ref.type != varTypeTraits<T>::type)
        return kVarTypeError;
    auto &val = *(var<T>*)(ref.self);
    varStatus status = val.set(value);
    if (status == kVarSuccess && callback)
        val();
    return status;
}

varStatus varChange(const u::string &name, const u::string &value, bool callback) {
    if (variables().find(name) == variables().end())
        return kVarNotFoundError;
    auto &ref = variables()[name];
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
            auto handle = (var<int>*)ref.self;
            auto v = handle->get();
            if (handle->flags() & kVarPersist)
                u::fprint(fp, "%s %d\n", name, v);
        } else if (ref.type == kVarFloat) {
            auto handle = (var<float>*)ref.self;
            auto v = handle->get();
            if (handle->flags() & kVarPersist)
                u::fprint(fp, "%s %.2f\n", name, v);
        } else if (ref.type == kVarString) {
            auto handle = (var<u::string>*)ref.self;
            auto v = handle->get();
            if (handle->flags() & kVarPersist) {
                if (v.empty())
                    return;
                u::fprint(fp, "%s \"%s\"\n", name, v.c_str());
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
