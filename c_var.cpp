#include "c_var.h"
#include "u_file.h"
#include "util.h"
#include "engine.h"

namespace c {

struct varReference {
    varReference(void) { }
    varReference(const char *desc, void *self, varType type) :
        desc(desc),
        self(self),
        type(type)
    { }

    const char *desc;
    void *self;
    varType type;
};

static u::map<u::string, varReference> &variables(void) {
    static u::map<u::string, varReference> *vars = new u::map<u::string, varReference>();
    return *vars;
}

// public API
void varRegister(const char *name, const char *desc, void *self, varType type) {
    if (variables().find(name) != variables().end())
        return;
    variables().emplace(name, varReference(desc, self, type));
}

template <typename T>
static inline void varSet(const u::string &name, const T &value) {
    if (variables().find(name) == variables().end())
        return;
    auto &ref = variables()[name];
    if (ref.type != varTypeTraits<T>::type)
        return;

    typedef var<T> type;
    type *var = reinterpret_cast<type*>(ref.self);
    var->set(value);
}

void varChange(const u::string &name, const u::string &value) {
    if (variables().find(name) == variables().end())
        return;
    auto &ref = variables()[name];
    if (ref.type == VAR_INT) {
        varSet<int>(name, u::atoi(value));
    } else if (ref.type == VAR_FLOAT) {
        varSet<float>(name, u::atof(value));
    }
}

bool writeConfig(void) {
    u::file file = u::fopen(neoPath() + "init.cfg", "w");
    if (!file)
        return false;
    auto writeLine = [](FILE *fp, const u::string &name, const varReference &ref) {
        if (ref.type == VAR_INT) {
            typedef var<int> type;
            type &v = *reinterpret_cast<type*>(ref.self);
            fprintf(fp, "%s %d\n", name.c_str(), v.get());
        } else if (ref.type == VAR_FLOAT) {
            typedef var<float> type;
            type &v = *reinterpret_cast<type*>(ref.self);
            fprintf(fp, "%s %.2f\n", name.c_str(), v.get());
        }
    };
    for (auto &it : variables())
        writeLine(file, it.first, it.second);
    return true;
}

bool readConfig(void) {
    u::file file = u::fopen(neoPath() + "init.cfg", "r");
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
