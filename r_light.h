#ifndef R_LIGHT_HDR
#define R_LIGHT_HDR
#include "r_method.h"
#include "r_gbuffer.h"

#include "m_vec3.h"

struct directionalLight;
struct pointLight;
struct spotLight;

namespace m {
    struct mat4;
    struct perspectiveProjection;
}

namespace r {

struct lightMethod : method {
    bool init(const char *vs, const char *fs, const u::vector<const char *> &defines = u::vector<const char *>());

    enum {
        // First three must have same layout as gBuffer
        kColor  = gBuffer::kColor,
        kNormal = gBuffer::kNormal,
        kDepth  = gBuffer::kDepth,
        kOcclusion
    };

    void setWVP(const m::mat4 &wvp);
    void setInverse(const m::mat4 &inverse);
    void setColorTextureUnit(int unit);
    void setNormalTextureUnit(int unit);
    void setDepthTextureUnit(int unit);
    void setOcclusionTextureUnit(int unit);
    void setEyeWorldPos(const m::vec3 &position);
    void setPerspectiveProjection(const m::perspectiveProjection &project);

private:
    GLint m_WVPLocation;
    GLint m_inverseLocation;

    GLint m_colorTextureUnitLocation;
    GLint m_normalTextureUnitLocation;
    GLint m_depthTextureUnitLocation;
    GLint m_occlusionTextureUnitLocation;

    GLint m_eyeWorldPositionLocation;

    GLint m_screenSizeLocation;
    GLint m_screenFrustumLocation;
};

struct directionalLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const directionalLight &light);

private:
    struct {
        GLint color;
        GLint ambient;
        GLint diffuse;
        GLint direction;
    } m_directionalLightLocation;
};

struct pointLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const pointLight &light);

private:
    struct {
        GLint color;
        GLint ambient;
        GLint diffuse;
        GLint position;
        GLint radius;
    } m_pointLightLocation;
};

struct spotLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const spotLight &light);

private:
    struct {
        GLint color;
        GLint ambient;
        GLint diffuse;
        GLint position;
        GLint radius;
        GLint direction;
        GLint cutOff;
    } m_spotLightLocation;
};

}

#endif
