#ifndef R_SSAO_HDR
#define R_SSAO_HDR

#include "r_common.h"
#include "m_mat4.h"

namespace r {

struct ssao {
    ssao();
    ~ssao();

    enum textureType {
        kBuffer,
        kRandom
    };

    bool init(const m::perspectiveProjection &project);
    void update(const m::perspectiveProjection &project);
    void bindWriting();

    GLuint texture(textureType unit) const;

private:
    void destroy();

    static constexpr size_t kRandomSize = 128;

    GLuint m_fbo;
    GLuint m_textures[2];
    size_t m_width;
    size_t m_height;
};

}

#endif
