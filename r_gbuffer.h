#ifndef R_GBUFFER_HDR
#define R_GBUFFER_HDR
#include "r_common.h"

namespace m {
    struct perspectiveProjection;
}

namespace r {

struct gBuffer {
    // also the texture unit order
    enum textureType {
        kColor,
        kNormal,
        kDepth,
        kMax
    };

    gBuffer();
    ~gBuffer();

    bool init(const m::perspectiveProjection &project);

    void update(const m::perspectiveProjection &project);
    void bindReading();
    void bindWriting();

    GLuint texture(textureType type) const;

private:
    void destroy();

    GLuint m_fbo;
    GLuint m_textures[kMax];
    size_t m_width;
    size_t m_height;
};


}

#endif
