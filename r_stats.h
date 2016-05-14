#ifndef R_STATS_HDR
#define R_STATS_HDR
#include "u_map.h"

namespace r {

struct stat {
    stat();
    stat(const char *name, const char *description);

    void decInstances();
    void adjustVBOMemory(int amount);
    void adjustIBOMemory(int amount);
    void adjustTextureCount(int amount);
    void adjustTextureMemory(int amount);

    const char *description() const;
    const char *name() const;

    static void render(size_t x);
    static stat *add(const char *name, const char *description);

    static stat *get(const char *name);

private:
    size_t draw(size_t x, size_t y) const;
    size_t space() const;

    const char *m_description;
    const char *m_name;

    size_t m_vboMemory;
    size_t m_iboMemory;
    size_t m_textureCount;
    size_t m_textureMemory;
    size_t m_instances;

    static u::map<const char *, stat> m_stats;
};

inline stat::stat()
    : m_description(nullptr)
    , m_name(nullptr)
    , m_vboMemory(0)
    , m_iboMemory(0)
    , m_textureCount(0)
    , m_textureMemory(0)
    , m_instances(0)
{
}

inline stat::stat(const char *name, const char *description)
    : stat()
{
    m_name = name;
    m_description = description;
    m_instances = 1;
}

inline void stat::decInstances() {
    if (m_instances)
        m_instances--;
}

inline void stat::adjustVBOMemory(int amount) {
    m_vboMemory += amount;
}

inline void stat::adjustIBOMemory(int amount) {
    m_iboMemory += amount;
}

inline void stat::adjustTextureCount(int amount) {
    m_textureCount += amount;
}

inline void stat::adjustTextureMemory(int amount) {
    m_textureMemory += amount;
}

inline const char *stat::description() const {
    return m_description;
}

inline const char *stat::name() const {
    return m_name;
}

}

#endif
