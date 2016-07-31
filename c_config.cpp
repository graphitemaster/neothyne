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

    typedef u::pair<u::string, u::string> line;
    u::vector<line> lines;
    auto addLine = [&lines](const Reference &ref) {
        const char *name = ref.m_name;
        if (ref.m_type == kVarInt) {
            const auto handle = (const Variable<int>*)ref.m_handle;
            const auto &value = handle->get();
            if (handle->flags() & kPersist)
                lines.push_back({ name, u::format("%d", value) });
        } else if (ref.m_type == kVarFloat) {
            const auto handle = (const Variable<float>*)ref.m_handle;
            const auto &value = handle->get();
            if (handle->flags() & kPersist)
                lines.push_back({ name, u::format("%.2f", value) });
        } else if (ref.m_type == kVarString) {
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

            const auto handle = (const Variable<u::string>*)ref.m_handle;
            const auto &value = handle->get();
            if (handle->flags() & kPersist) {
                if (value.empty())
                    return;
                lines.push_back({ name, u::format("\"%s\"", escape(value)) });
            }
        }
    };

    // use the static linked list for traversal: it's sorted by translation unit
    // which means sorting below will take less effort as it's already partially
    // sorted. This is also quicker to traverse than the hash table
    for (Reference *ref = Console::m_references; ref; ref = ref->m_next)
        addLine(*ref);

    // sort all lines lexicographically before writing
    u::sort(lines.begin(), lines.end(),
        [](const line &a, const line &b) { return a.first < b.first; });

    for (const auto &it : lines)
        u::fprint(file, "%s %s\n", it.first, it.second);

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
