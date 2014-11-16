#ifndef R_GEOM_HDR
#define R_GEOM_HDR
#include "r_common.h"

namespace r {

struct geom {
    geom();
    ~geom();
    void render();

private:
    friend struct quad;
    friend struct sphere;

    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };
    GLuint m_vao;
    GLenum m_mode;
    GLsizei m_count;
    GLenum m_type;
};

struct quad : geom {
    bool upload();
};

struct sphere : geom {
    bool upload();
private:
    static constexpr size_t kSlices = 8;
    static constexpr size_t kStacks = 4;
};

}

#endif
