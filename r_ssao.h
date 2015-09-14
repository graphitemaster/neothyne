#ifndef R_SSAO_HDR
#define R_SSAO_HDR
#include "r_method.h"

#include "m_mat.h"

namespace r {

struct ssaoMethod : method {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    // Note if you change this you'll need to update the shader since all
    // of the `kKernelSize' iterations are unrolled
    static constexpr size_t kKernelSize = 4;

    enum {
        kNormal,
        kDepth,
        kRandom
    };

    void setOccluderBias(float bias);
    void setSamplingRadius(float radius);
    void setAttenuation(float constant, float linear);
    void setInverse(const m::mat4 &inverse);
    void setWVP(const m::mat4 &wvp);
    void setPerspective(const m::perspective &p);
    void setNormalTextureUnit(int unit);
    void setDepthTextureUnit(int unit);
    void setRandomTextureUnit(int unit);

private:
    uniform *m_occluderBias;
    uniform *m_samplingRadius;
    uniform *m_attenuation;
    uniform *m_inverse;
    uniform *m_WVP;
    uniform *m_screenFrustum;
    uniform *m_screenSize;
    uniform *m_normalTexture;
    uniform *m_depthTexture;
    uniform *m_randomTexture;
    uniform *m_kernel[kKernelSize];
};

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
