#ifndef R_SSAO_HDR
#define R_SSAO_HDR

#include "r_common.h"
#include "m_mat4.h"

namespace r {

struct ssao {
    ssao();
    ~ssao();

    bool init(const m::perspectiveProjection &project);
    void update(const m::perspectiveProjection &project);
    void bindWriting();

    GLuint texture() const;
private:
    void destroy();

    GLuint m_fbo;
    GLuint m_texture;
    size_t m_width;
    size_t m_height;
};

}

#endif
