#ifndef R_GBUFFER_HDR
#define R_GBUFFER_HDR
#include "r_common.h"

namespace m {
    struct perspective;
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

    bool init(const m::perspective &p);

    void update(const m::perspective &p);
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
