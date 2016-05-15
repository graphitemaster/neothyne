#ifndef R_STATS_HDR
#define R_STATS_HDR
#include "u_map.h"

namespace r {

struct stat {
    stat();
    stat(const char *name, const char *description);

    void incInstances();
    void decInstances();
    void incVBOMemory(int amount);
    void decVBOMemory(int amount);
    void incIBOMemory(int amount);
    void decIBOMemory(int amount);
    void incTextureCount();
    void decTextureCount();
    void incTextureMemory(int amount);
    void decTextureMemory(int amount);

    const char *description() const;
    const char *name() const;

    static void render(size_t x);
    static stat *add(const char *name, const char *description);

    static stat *get(const char *name);

private:
    static void drawHistogram(size_t x, size_t next);
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
    static u::vector<float> m_histogram;
    static u::vector<unsigned char> m_texture;
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

inline void stat::incInstances() {
    m_instances++;
}

inline void stat::decInstances() {
    if (m_instances)
        m_instances--;
}

inline void stat::incVBOMemory(int amount) {
    m_vboMemory += amount;
}

inline void stat::decVBOMemory(int amount) {
    m_vboMemory -= amount;
}

inline void stat::incIBOMemory(int amount) {
    m_iboMemory += amount;
}

inline void stat::decIBOMemory(int amount) {
    m_iboMemory -= amount;
}

inline void stat::incTextureCount() {
    m_textureCount++;
}

inline void stat::decTextureCount() {
    if (m_textureCount)
        m_textureCount--;
}

inline void stat::incTextureMemory(int amount) {
    m_textureMemory += amount;
}

inline void stat::decTextureMemory(int amount) {
    m_textureMemory -= amount;
}

inline const char *stat::description() const {
    return m_description;
}

inline const char *stat::name() const {
    return m_name;
}

}

#endif
