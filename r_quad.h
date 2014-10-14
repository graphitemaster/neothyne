#ifndef R_QUAD_HDR
#define R_QUAD_HDR
#include "r_common.h"

namespace r {

struct quad {
    ~quad();

    bool upload(void);
    void render(void);

private:
    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };
    GLuint m_vao;
};

}

#endif
