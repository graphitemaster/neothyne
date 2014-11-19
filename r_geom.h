#ifndef R_GEOM_HDR
#define R_GEOM_HDR
#include "r_common.h"

namespace r {

struct geom {
    geom();
    ~geom();
    void render();
    void upload();

    union {
        struct {
            GLuint vbo;
            GLuint ibo;
        };
        GLuint buffers[2];
    };
    GLuint vao;
    GLenum mode;
    GLsizei count;
    GLenum type;
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
