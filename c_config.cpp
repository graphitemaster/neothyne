#include "u_algorithm.h"
#include "u_vector.h"
#include "u_file.h"
#include "u_misc.h"

#include "c_config.h"
#include "c_console.h"

namespace c {

bool Config::write(const u::string &path) {
    u::file file = u::fopen(path + "init.cfg", "w");
    if (!file)
        return false;

    // use the static linked list for traversal it's sorted
    for (Reference *ref = Console::m_references; ref; ref = ref->m_next) {
        const char *name = ref->m_name;
        if (ref->m_type == kVarInt) {
            const auto handle = (const Variable<int>*)ref->m_handle;
            const auto &value = handle->get();
            if (handle->flags() & kPersist)
                u::fprint(file, "%s %d\n", name, value);
        } else if (ref->m_type == kVarFloat) {
            const auto handle = (const Variable<float>*)ref->m_handle;
            const auto &value = handle->get();
            if (handle->flags() & kPersist)
                u::fprint(file, "%s %.2f\n", name, value);
        } else if (ref->m_type == kVarString) {
            auto escape = [](const u::string &before) {
                u::string after;
                after.reserve(before.size() + 4);
                for (size_t i = 0; i < before.size(); i++) {
                    switch (before[i]) {
                    case '"':
                    case '\\':
                        after += '\\';
                    default:
                        after += before[i];
                    }
                }
                return after;
            };

            const auto handle = (const Variable<u::string>*)ref->m_handle;
            const auto &value = handle->get();
            if (handle->flags() & kPersist) {
                if (value.empty())
                    continue;
                u::fprint(file, "%s \"%s\"\n", name, escape(value));
            }
        }
    }

    return true;
}

bool Config::read(const u::string &path) {
    u::file file = u::fopen(path + "init.cfg", "r");
    if (!file)
        return false;

    for (u::string line; u::getline(file, line); ) {
        const u::vector<u::string> kv = u::split(line);
        if (kv.size() != 2)
            continue;
        const u::string &key = kv[0];
        const u::string &value = kv[1];
        if (Console::change(key, value) != Console::kVarSuccess)
            return false;
    }
    return true;
}

}
