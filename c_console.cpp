#include <errno.h>
#include <limits.h>

#include "u_misc.h"

#include "c_console.h"
#include "c_complete.h"

namespace c {

alignas(Console::Map)
unsigned char Console::m_mapData[sizeof(Map)];
Console::Map *Console::m_map;

alignas(Complete)
unsigned char Console::m_completeData[sizeof(Complete)];
Complete *Console::m_complete;

// Console variables are arranged into a linked list of references with the use
// of static constructors. The initialization function then initializes a
// hashtable and inserts them while initializing string variables. This avoids
// having to lazily initialize the string heap and helps with memory management
// during shutdown
Reference *Console::m_references = nullptr;

const Reference &Console::reference(const char *name) {
    const auto &table = map();
    const auto &find = table.find(name);
    U_ASSERT(find != table.end());
    return find->second;
}

template <typename T>
Variable<T> &Console::value(const char *name) {
    auto &ref = reference(name);
    return *(Variable<T> *)ref.m_handle;
}

template Variable<int> &Console::value<int>(const char *name);
template Variable<float> &Console::value<float>(const char *name);
template Variable<u::string> &Console::value<u::string>(const char *name);

u::optional<u::string> Console::value(const char *name) {
    auto &ref = reference(name);
    switch (ref.m_type) {
    case kVarFloat:
        return u::format("%.2f", ((Variable<float>*)ref.m_handle)->get());
    case kVarInt:
        return u::format("%d", ((Variable<int>*)ref.m_handle)->get());
    case kVarString:
        return u::format("%s", ((Variable<u::string>*)ref.m_handle)->get());
    }
    return u::none;
}

template <typename T>
int Console::set(const char *name, const T &value) {
    const auto &table = map();
    const auto &find = table.find(name);
    if (find == table.end())
        return kVarNotFoundError;
    auto &ref = find->second;
    if (ref.m_type != Trait<T>::value)
        return kVarTypeError;
    auto &variable = *(Variable<T>*)ref.m_handle;
    return variable.set(value);
}

template int Console::set<int>(const char *name, const int &value);
template int Console::set<float>(const char *name, const float &value);
template int Console::set<u::string>(const char *name, const u::string &value);

int Console::change(const char *name, const char *value) {
    const auto &table = map();
    const auto &find = table.find(name);
    if (find == table.end())
        return kVarNotFoundError;

    auto scanInt = [](const char *name, const char *input) -> int {
        char *scan = (char *)input;
        long value = strtol(input, &scan, 10);
        if (errno != 0)
            return kVarRangeError;
        if (scan == input || !*scan != '\0')
            return kVarMalformedError;
        if (value < INT_MIN || value > INT_MAX)
            return kVarRangeError;
        return set<int>(name, (int)value);
    };

    auto scanFloat = [](const char *name, const char *input) -> int {
        char *scan = (char *)input;
        float value = strtof(input, &scan);
        if (errno != 0)
            return kVarRangeError;
        if (scan == input || !*scan != '\0')
            return kVarMalformedError;
        return set<float>(name, value);
    };

    const auto &ref = find->second;
    switch (ref.m_type) {
    case kVarInt:
        return scanInt(name, value);
    case kVarFloat:
        return scanFloat(name, value);
    case kVarString:
        if (value[0] == '"' && value[strlen(value)-1] == '"') {
            u::string copy(value + 1);
            copy.pop_back();
            return set<u::string>(name, &copy[0]);
        }
        return set<u::string>(name, value);
    }
    return kVarTypeError;
}

int Console::change(const u::string &name, const u::string &value) {
    return change(&name[0], &value[0]);
}

Reference *Console::split(Reference *ref) {
    if (!ref || !ref->m_next)
        return nullptr;
    Reference *splitted = ref->m_next;
    ref->m_next = splitted->m_next;
    splitted->m_next = split(splitted->m_next);
    return splitted;
}

Reference *Console::merge(Reference *lhs, Reference *rhs) {
    if (!lhs)
        return rhs;
    if (!rhs)
        return lhs;
    if (strcmp(lhs->m_name, rhs->m_name) > 0) {
        rhs->m_next = merge(lhs, rhs->m_next);
        return rhs;
    }
    lhs->m_next = merge(lhs->m_next, rhs);
    return lhs;
}

Reference *Console::sort(Reference *begin) {
    if (!begin)
        return nullptr;
    if (!begin->m_next)
        return begin;
    Reference *splitted = split(begin);
    return merge(sort(begin), sort(splitted));
}

void Console::initialize() {
    // sort the references by key
    m_references = sort(m_references);
    m_map = new (m_mapData) Map;
    m_complete = new (m_completeData) Complete;

    auto &table = map();
    auto &completer = complete();

    for (Reference *ref = m_references; ref; ref = ref->m_next) {
        table[ref->m_name] = *ref;
        // initialize the string values
        if (ref->m_type == kVarString) {
            auto &value = *((Variable<u::string> *)ref->m_handle);
            value.m_current = new (value.m_currentData) u::string(value.m_default ? value.m_default : "");
        }
        // add it to the auto complete tree
        completer.insert(ref->m_name);
    }
}

void Console::shutdown() {
    // be sure to call the destructors on the strings
    for (Reference *ref = m_references; ref; ref = ref->m_next) {
        if (ref->m_type != kVarString)
            continue;
        auto &value = *((Variable<u::string> *)ref->m_handle);
        value.get().~string();
    }
    // and the hashtable
    m_map->~Map();
    // and the auto complete tree
    m_complete->~Complete();
}

u::vector<u::string> Console::suggestions(const u::string &prefix) {
    u::vector<u::string> matches;
    complete().search(prefix, matches);
    return u::move(matches);
}

Console::Map &Console::map() {
    return *m_map;
}

Complete &Console::complete() {
    return *m_complete;
}

}
