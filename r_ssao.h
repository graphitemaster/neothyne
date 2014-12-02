#ifndef R_SSAO_HDR
#define R_SSAO_HDR

#include "r_method.h"

#include "m_mat4.h"

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
    void setPerspectiveProjection(const m::perspectiveProjection &project);
    void setNormalTextureUnit(int unit);
    void setDepthTextureUnit(int unit);
    void setRandomTextureUnit(int unit);

private:
    GLint m_occluderBiasLocation;
    GLint m_samplingRadiusLocation;
    GLint m_attenuationLocation;
    GLint m_inverseLocation;
    GLint m_WVPLocation;
    GLint m_screenFrustumLocation;
    GLint m_screenSizeLocation;
    GLint m_normalTextureLocation;
    GLint m_depthTextureLocation;
    GLint m_randomTextureLocation;
    GLint m_kernelLocation[kKernelSize];
};

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
