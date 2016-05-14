#ifndef R_BILLBOARD_HDR
#define R_BILLBOARD_HDR
#include "r_geom.h"
#include "r_texture.h"
#include "r_method.h"
#include "r_stats.h"

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
    billboard();
    ~billboard();

    bool load(const u::string &billboardTexture);
    bool upload();

    void render(const pipeline &pl, float size);

    enum {
        kSide = 1 << 1,
        kUp = 1 << 2
    };

    // you must add all positions for this billboard before calling `upload'
    void add(const m::vec3 &position,
             int flags = kSide | kUp,
             const m::vec3 &optionalSide = m::vec3::origin,
             const m::vec3 &optionalUp = m::vec3::origin);

    const char *description() const;
    size_t memory() const;

private:
    struct vertex {
        m::vec3 position;
        m::vec2 coordinate;
    };

    struct entry {
        m::vec3 position;
        int flags;
        m::vec3 side;
        m::vec3 up;
    };

    u::vector<entry> m_entries;
    u::vector<vertex> m_vertices;
    u::vector<GLuint> m_indices;
    texture2D m_texture;
    billboardMethod m_method;
    r::stat *m_stats;
};

}

#endif
