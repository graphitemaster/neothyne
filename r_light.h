#ifndef R_LIGHT_HDR
#define R_LIGHT_HDR
#include "r_method.h"
#include "r_gbuffer.h"

struct directionalLight;
struct pointLight;
struct spotLight;
struct fog;

namespace m {
    struct mat4;
    struct perspective;
}

namespace r {

struct lightMethod : method {
    lightMethod();

    bool init(const char *vs,
              const char *fs,
              const char *description,
              const u::vector<const char *> &defines = u::vector<const char *>());

    enum {
        // First three must have same layout as gBuffer
        kColor  = gBuffer::kColor,
        kNormal = gBuffer::kNormal,
        kDepth  = gBuffer::kDepth,
        kShadowMap,
        kOcclusion
    };

    void setWVP(const m::mat4 &wvp);
    void setInverse(const m::mat4 &inverse);
    void setColorTextureUnit(int unit);
    void setNormalTextureUnit(int unit);
    void setDepthTextureUnit(int unit);
    void setShadowMapTextureUnit(int unit);
    void setOcclusionTextureUnit(int unit);
    void setEyeWorldPos(const m::vec3 &position);
    void setPerspective(const m::perspective &p);

private:
    uniform *m_WVP;
    uniform *m_inverse;

    uniform *m_colorTextureUnit;
    uniform *m_normalTextureUnit;
    uniform *m_depthTextureUnit;
    uniform *m_shadowMapTextureUnit;
    uniform *m_occlusionTextureUnit;

    uniform *m_eyeWorldPosition;

    uniform *m_screenSize;
    uniform *m_screenFrustum;
};

struct directionalLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const directionalLight &light);
    void setFog(const fog &f);

private:
    struct {
        uniform *color;
        uniform *ambient;
        uniform *diffuse;
        uniform *direction;
    } m_directionalLight;

    struct {
        uniform *color;
        uniform *density;
        uniform *range;
        uniform *equation;
    } m_fog;
};

struct pointLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const pointLight &light);
    void setLightWVP(const m::mat4 &wvp);

private:
    struct {
        uniform *color;
        uniform *ambient;
        uniform *diffuse;
        uniform *position;
        uniform *radius;
    } m_pointLight;

    uniform *m_lightWVP;
};

struct spotLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const spotLight &light);
    void setLightWVP(const m::mat4 &wvp);

private:
    struct {
        uniform *color;
        uniform *ambient;
        uniform *diffuse;
        uniform *position;
        uniform *radius;
        uniform *direction;
        uniform *cutOff;
    } m_spotLight;

    uniform *m_lightWVP;
};

}

#endif
