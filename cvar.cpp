#include "engine.h"
#include "cvar.h"

#include "c_complete.h"

#include "u_file.h"
#include "u_memory.h"

struct varReference;

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

static u::deferred_data<u::map<u::string, varReference>> gVariables;

// public API
void varRegister(const char *name, const char *desc, void *self, varType type) {
    if (gVariables()->find(name) != gVariables()->end())
        return;
    (*gVariables())[name] = varReference(desc, self, type);
    c::Complete::insert(name);
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
        // trim quotted string as well
        if (value[0] == '"' && value[value.size()-1] == '"') {
            u::string copy(value);
            copy.pop_front();
            copy.pop_back();
            return varSet<u::string>(name, u::move(copy), callback);
        }
        return varSet<u::string>(name, value, callback);
    }
    return kVarTypeError;
}

bool writeConfig(const u::string &userPath) {
    u::file file = u::fopen(userPath + "init.cfg", "w");
    if (!file)
        return false;

    typedef u::pair<u::string, u::string> line;
    u::vector<line> lines;
    auto addLine = [&lines](const u::string &name, const varReference &ref) {
        if (ref.type == kVarInt) {
            auto handle = (var<int>*)ref.self;
            const auto &v = handle->get();
            if (handle->flags() & kVarPersist)
                lines.push_back({ name, u::format("%d", v) });
        } else if (ref.type == kVarFloat) {
            auto handle = (var<float>*)ref.self;
            const auto &v = handle->get();
            if (handle->flags() & kVarPersist)
                lines.push_back({ name, u::format("%.2f", v) });
        } else if (ref.type == kVarString) {
            auto handle = (var<u::string>*)ref.self;
            const auto &v = handle->get();
            if (handle->flags() & kVarPersist) {
                if (v.empty())
                    return;
                lines.push_back({ name, u::format("\"%s\"", v) });
            }
        }
    };

    for (const auto &it : *gVariables())
        addLine(it.first, it.second);

    // sort all lines lexicographically before writing
    u::sort(lines.begin(), lines.end(),
        [](const line &a, const line &b) { return a.first < b.first; });

    for (const auto &it : lines)
        u::fprint(file, "%s %s\n", it.first, it.second);

    return true;
}

bool readConfig(const u::string &userPath) {
    u::file file = u::fopen(userPath + "init.cfg", "r");
    if (!file)
        return false;
    for (u::string line; u::getline(file, line); ) {
        const u::vector<u::string> kv = u::split(line);
        if (kv.size() != 2)
            continue;
        const u::string &key = kv[0];
        const u::string &value = kv[1];
        if (varChange(key, value) != kVarSuccess)
            return false;
    }
    return true;
}
