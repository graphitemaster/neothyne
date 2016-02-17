#ifndef R_COMPOSITE_HDR
#define R_COMPOSITE_HDR
#include "r_method.h"

namespace m {

struct mat4;
struct perspective;

}

namespace r {

struct composite {
    composite();
    ~composite();

    bool init(const m::perspective &p, GLuint depth);
    void update(const m::perspective &p);
    void bindWriting();

    GLuint texture() const;

private:
    enum {
        kBuffer,
        kDepth
    };

    void destroy();

    GLuint m_fbo;
    GLuint m_texture;
    size_t m_width;
    size_t m_height;
};

}

#endif
