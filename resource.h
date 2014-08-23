#ifndef RESOURCE_HDR
#define RESOURCE_HDR
#include "util.h"

template <typename K, typename T>
struct resourceManager {
    resourceManager() :
        m_loaded(0),
        m_reuses(0) { }

    ~resourceManager() {
        clear();
    }

    T *get(const K& key) {
        if (m_resources.find(key) != m_resources.end()) {
            m_reuses++;
            return m_resources[key];
        }
        T *resource = new T;
        if (!resource->load(key)) {
            delete resource;
            return nullptr;
        }
        m_resources[key] = resource;
        m_loaded++;
        return m_resources[key];
    }

    void clear(void) {
        for (auto &it : m_resources)
            delete it.second;
        m_resources.clear();
    }

    size_t size(void) const {
        return m_resources.size();
    }

    size_t loaded(void) const {
        return m_loaded;
    }

    size_t reuses(void) const {
        return m_reuses;
    }

private:
    u::map<K, T*> m_resources;
    size_t m_loaded; // how many resources are loaded
    size_t m_reuses; // how many resources where reused
};

#endif
