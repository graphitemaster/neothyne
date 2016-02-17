#ifndef R_SSAO_HDR
#define R_SSAO_HDR
#include "r_method.h"

#include "m_mat.h"

namespace r {

struct ssao {
    ssao();
    ~ssao();

    enum textureType {
        kBuffer,
        kRandom
    };

    bool init(const m::perspective &p);
    void update(const m::perspective &p);
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
