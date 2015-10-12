#ifndef R_BILLBOARD_HDR
#define R_BILLBOARD_HDR
#include "r_geom.h"
#include "r_texture.h"
#include "r_method.h"

#include "m_vec.h"

namespace u {
    struct string;
}

namespace m {
    struct mat4;
}

namespace r {

struct pipeline;

struct billboardMethod : method {
    billboardMethod();

    bool init();

    void setVP(const m::mat4 &vp);
    void setColorTextureUnit(int unit);

private:
    uniform *m_VP;
    uniform *m_colorMap;
};

struct billboard : geom {
    billboard(const char *description);
    bool load(const u::string &billboardTexture);
    bool upload();

    void render(const pipeline &pl, float size);

    // you must add all positions for this billboard before calling `upload'
    void add(const m::vec3 &position);

    const char *description() const;
    size_t memory() const;

private:
    struct vertex {
        m::vec3 p;
        float u, v;
    };
    u::vector<m::vec3> m_positions;
    u::vector<vertex> m_vertices;
    texture2D m_texture;
    billboardMethod m_method;
    size_t m_memory;
    const char *m_description;
};

inline billboard::billboard(const char *description)
    : m_memory(0)
    , m_description(description)
{
}

inline const char *billboard::description() const {
    return m_description;
}

inline size_t billboard::memory() const {
    return m_memory;
}

}

#endif
